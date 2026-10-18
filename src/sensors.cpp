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
    // KNOWN DEFECT (found during the code review, fix pending): this wait has no
    // deadline. The read loop above also ends on EOF, so a child that closes its
    // stdout without exiting (e.g. a wrapper that redirects and keeps running)
    // never trips timed_out and blocks this thread here forever. Measured: a
    // 3 s child with stdout closed and a 500 ms budget blocked for the full 3 s.
    // For the sampler that means nvidia-smi hangs stop all fan adjustment.
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    ::close(fds[0]);

    if (timed_out || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || out.empty())
        return std::nullopt;
    return out;
}

// A client counts as a reason to keep querying unless its reported name matches
// one of the exclusion substrings. nvidia-smi may report either a bare name
// (glmark2) or a full path (/usr/lib/Xorg), so substring is the only rule that
// covers both.
bool any_client_in_use(const std::vector<std::string>& names,
                       const std::vector<std::string>& exclude) {
    for (const std::string& name : names) {
        bool excluded = false;
        for (const std::string& pat : exclude) {
            if (!pat.empty() && name.find(pat) != std::string::npos) {
                excluded = true;
                break;
            }
        }
        if (!excluded)
            return true;
    }
    return false;
}

// One block per GPU:
//     Processes                                          : None
//     Processes
//         Process ID                        : 19899
//             Type                          : G
//             Name                          : glmark2
//             Used GPU Memory               : 166 MiB
// Nullopt when no Processes section was present at all: that output is not the
// shape we expect and must never be read as "zero clients".
std::optional<GpuProcList> parse_pids(std::string_view out,
                                      const std::vector<std::string>& exclude) {
    GpuProcList list;
    bool seen_section = false;
    bool in_section = false;

    std::size_t pos = 0;
    while (pos <= out.size()) {
        std::size_t nl = out.find('\n', pos);
        std::string_view line =
            out.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = (nl == std::string_view::npos) ? out.size() + 1 : nl + 1;

        std::string_view sv = line;
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
            sv.remove_prefix(1);

        if (sv.starts_with("Unidentified processes found")) {
            list.unidentified = true;
            seen_section = true;
            continue;
        }
        if (sv.starts_with("Processes")) {
            // The label is "Processes" alone or "Processes <pad> : None". A
            // longer word that merely starts the same way is not the section.
            std::string_view rest = sv.substr(9);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
                rest.remove_prefix(1);
            if (rest.empty() || rest.front() == ':') {
                seen_section = true;
                in_section = true; // "Processes : None" simply adds no name
                continue;
            }
        }
        if (!in_section || !sv.starts_with("Name"))
            continue;

        // Only the "Name <padding> : value" field counts; a process whose name
        // itself starts with "Name" sits after the colon, not at column 0.
        auto colon = sv.find(':');
        if (colon == std::string_view::npos)
            continue;
        bool only_pad = true;
        for (char c : sv.substr(4, colon - 4)) {
            if (c != ' ' && c != '\t') {
                only_pad = false;
                break;
            }
        }
        if (!only_pad)
            continue;

        std::string_view val = sv.substr(colon + 1);
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t' || val.front() == '\r'))
            val.remove_prefix(1);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r'))
            val.remove_suffix(1);
        list.names.emplace_back(val);
    }

    if (!seen_section)
        return std::nullopt;
    list.in_use = any_client_in_use(list.names, exclude);
    return list;
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
    // Intel only, on purpose. coretemp exposes the package temperature as
    // temp1_input, which is what the control law's target_temp is calibrated
    // against.
    // AMD is deliberately not accepted: on k10temp temp1_input is Tctl, which
    // carries a model-dependent offset, so the same target_temp would select a
    // different real junction temperature. That difference was not measurable
    // here (no AMD machine), so the project decided not to claim AMD support:
    // with no coretemp device this returns nullopt and probe_sensors() exits.
    // Revisit only with AMD hardware to compare Tctl against Tdie.
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec)) {
        auto name = read_line(entry.path().string() + "/name");
        if (!name)
            continue;
        if (*name != "coretemp")
            continue;

        CpuTemp t;
        t.input_path_ = entry.path().string() + "/temp1_input";
        if (t.read())
            return t;
    }
    log("CPU temp: no coretemp hwmon device found");
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
    // Only the CPU-only/GPU+CPU mode switch depends on this path.
    // UNCERTAIN, needs a decision on multi-GPU machines: this returns the first
    // NVIDIA display controller in directory-iteration order (PCI address order)
    // while nvidia-smi defaults to its own GPU 0. With more than one NVIDIA GPU
    // the two may name different cards, so the mode flag could follow one card
    // while the temperature and power readings come from another. Unverified:
    // only a single-dGPU machine was available.
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
    // A successful parse is all that is checked: a negative or absurd value from
    // the driver is passed through. Harmless today, because the value only feeds
    // the average, but the averaging is not filtered either.

    return std::pair{temp, power};
}

std::optional<GpuProcList> nvidia_processes(const std::vector<std::string>& exclude) {
    auto out = run_capture("nvidia-smi -q -d PIDS", kNvidiaSmiTimeoutMs);
    if (!out)
        return std::nullopt;
    return parse_pids(*out, exclude);
}

} // namespace cfc
