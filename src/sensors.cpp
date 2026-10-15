#include "sensors.hpp"

#include "log.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <string_view>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace cfc {
namespace {

// Reads a single whitespace-delimited value from a sysfs file.
std::optional<std::string> read_line(const std::string& path) {
    std::ifstream f(path);
    if (!f)
        return std::nullopt;
    std::string line;
    if (!std::getline(f, line))
        return std::nullopt;
    return line;
}

std::optional<uint64_t> read_u64(const std::string& path) {
    auto line = read_line(path);
    if (!line)
        return std::nullopt;
    std::string_view sv{*line};
    while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r'))
        sv.remove_suffix(1);
    uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size())
        return std::nullopt;
    return value;
}

// Runs a command and returns its stdout, killing it if it outlives timeout_ms.
// A hung nvidia-smi must not stall the sampler, which would freeze fan control.
std::optional<std::string> run_capture(const char* cmd, int timeout_ms) {
    int fds[2];
    if (::pipe(fds) != 0)
        return std::nullopt;

    ::pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return std::nullopt;
    }

    if (pid == 0) {
        // child: stdout becomes the pipe, stderr is discarded
        ::close(fds[0]);
        ::dup2(fds[1], STDOUT_FILENO);
        ::close(fds[1]);
        int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }
        ::execl("/bin/sh", "sh", "-c", cmd, static_cast<char*>(nullptr));
        ::_exit(127);
    }

    // parent: read with a deadline so a stuck child cannot block us
    ::close(fds[1]);
    std::string out;
    char buf[256];

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool timed_out = false;

    for (;;) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                        deadline - std::chrono::steady_clock::now())
                        .count();
        if (left <= 0) {
            timed_out = true;
            break;
        }

        pollfd pfd{};
        pfd.fd = fds[0];
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, static_cast<int>(left));
        if (pr < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (pr == 0) {
            timed_out = true;
            break;
        }

        ssize_t n = ::read(fds[0], buf, sizeof(buf));
        if (n > 0)
            out.append(buf, static_cast<std::size_t>(n));
        else if (n == 0)
            break; // EOF
        else if (errno != EINTR)
            break;
    }

    if (timed_out)
        ::kill(pid, SIGKILL);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    ::close(fds[0]);

    if (timed_out || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || out.empty())
        return std::nullopt;
    return out;
}

} // namespace

std::optional<RaplEnergy> RaplEnergy::open(const std::string& dir) {
    RaplEnergy e;
    e.energy_path_ = dir + "/energy_uj";

    auto max_range = read_u64(dir + "/max_energy_range_uj");
    if (!max_range || *max_range == 0) {
        logf("RAPL: cannot read max_energy_range_uj under {}", dir);
        return std::nullopt;
    }
    e.max_range_ = *max_range;

    if (!e.read())
        return std::nullopt;
    return e;
}

std::optional<uint64_t> RaplEnergy::read() {
    auto raw = read_u64(energy_path_);
    if (!raw)
        return std::nullopt;

    if (have_last_) {
        if (*raw < last_raw_)
            wraps_++; // counter wrapped between the two samples
        last_raw_ = *raw;
    } else {
        last_raw_ = *raw;
        have_last_ = true;
    }
    return wraps_ * max_range_ + *raw;
}

std::optional<CpuTemp> CpuTemp::open() {
    // Prefer hwmon: coretemp on Intel, k10temp on AMD. Both expose the package
    // temperature as temp1_input.
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec)) {
        auto name = read_line(entry.path().string() + "/name");
        if (!name)
            continue;
        if (*name != "coretemp" && *name != "k10temp")
            continue;

        CpuTemp t;
        t.input_path_ = entry.path().string() + "/temp1_input";
        if (t.read())
            return t;
    }
    log("CPU temp: no coretemp/k10temp hwmon device found");
    return std::nullopt;
}

std::optional<int> CpuTemp::read() const {
    // hwmon reports millidegrees Celsius
    auto milli = read_u64(input_path_);
    if (!milli)
        return std::nullopt;
    return static_cast<int>(*milli / 1000);
}

std::optional<std::string> find_nvidia_runtime_status() {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/bus/pci/devices", ec)) {
        auto vendor = read_line(entry.path().string() + "/vendor");
        if (!vendor)
            continue;
        // 0x10de is NVIDIA
        if (std::string_view{*vendor} != "0x10de")
            continue;

        // A GPU has several functions sharing the vendor ID: 01:00.1 is
        // typically the HDMI audio controller (class 0x040300). Only accept
        // display controllers, otherwise the wrong runtime_status may win.
        auto klass = read_line(entry.path().string() + "/class");
        if (!klass)
            continue;
        std::string_view k{*klass};
        if (k.substr(0, 4) != "0x03")
            continue;

        return entry.path().string() + "/power/runtime_status";
    }
    return std::nullopt;
}

std::optional<std::pair<int, double>> nvidia_readings() {
    // One call, two values, machine readable. No waking the GPU twice.
    auto out = run_capture("nvidia-smi --query-gpu=temperature.gpu,power.draw "
                           "--format=csv,noheader,nounits",
                           kNvidiaSmiTimeoutMs);
    if (!out)
        return std::nullopt;

    // expected: "45, 12.34"
    auto comma = out->find(',');
    if (comma == std::string::npos)
        return std::nullopt;

    std::string temp_s = out->substr(0, comma);
    std::string power_s = out->substr(comma + 1);

    int temp = 0;
    {
        auto [p, e] = std::from_chars(temp_s.data(), temp_s.data() + temp_s.size(), temp);
        if (e != std::errc{})
            return std::nullopt;
    }

    // power may read "[N/A]"; strtod leaves endptr at the start in that case
    char* end = nullptr;
    double power = std::strtod(power_s.c_str(), &end);
    if (end == power_s.c_str())
        return std::nullopt;

    return std::pair{temp, power};
}

} // namespace cfc
