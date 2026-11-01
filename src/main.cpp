#include "config.hpp"
#include "control.hpp"
#include "ec.hpp"
#include "log.hpp"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) {
    g_stop = 1;
}

// Returns the directory holding the running executable, so the config sits
// next to the binary regardless of the current working directory.
fs::path exe_dir() {
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec)
        return fs::current_path();
    return exe.parent_path();
}

} // namespace

int main() {
    using namespace std::chrono_literals;

    // The EC is reached through ioperm(), which needs CAP_SYS_RAWIO, and the CPU
    // energy counter is mode 0400. Both require root, so refuse to run without it.
    if (::geteuid() != 0) {
        cfc::error("must run as root");
        return 1;
    }

    const fs::path config_path = exe_dir() / "config.toml";
    auto cfg = cfc::load_config(config_path.string());
    if (!cfg) {
        cfc::errorf("configuration rejected: {}", config_path.string());
        return 1;
    }

    cfc::set_level(cfg->debug_log ? cfc::Level::Debug : cfc::Level::Norm);
    // Resolve the journal check once, before any threads exist.
    cfc::init_journal_detection();

    // ioperm() grants port access per thread and new threads inherit the
    // bitmap, so this must happen on the main thread before any are started.
    cfc::Ec ec;
    if (!ec.init())
        return 1;

    struct sigaction sa {};
    sa.sa_handler = on_signal;
    ::sigaction(SIGTERM, &sa, nullptr);
    ::sigaction(SIGINT, &sa, nullptr);

    cfc::Control control(*cfg, ec);

    // Probe the CPU sensors before starting anything: driving fans without a
    // reliable temperature is not safe, so a failure here is fatal.
    if (!control.probe_sensors()) {
        cfc::error("required sensors unavailable; refusing to control fans");
        return 1;
    }

    std::vector<std::jthread> threads;
    threads.emplace_back([&control](std::stop_token s) { control.sampler_loop(s); });
    threads.emplace_back([&control](std::stop_token s) { control.fan_loop(s, 1); });
    threads.emplace_back([&control](std::stop_token s) { control.fan_loop(s, 2); });

    cfc::log("started");
    while (g_stop == 0)
        std::this_thread::sleep_for(100ms);

    for (auto& t : threads)
        t.request_stop();
    threads.clear(); // jthread destructors join

    // Hand both fans back to the EC's own control, matching the old shutdown path.
    // Note: this runs only on the clean exit below. SIGKILL, a crash, or a panic
    // leaves the fans in fixed duty mode until the EC is reset, because nothing
    // else restores auto mode.
    ec.set_auto(1);
    ec.set_auto(2);
    cfc::log("stopped: both fans restored to EC auto");
    return 0;
}
