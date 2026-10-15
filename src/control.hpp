#pragma once

#include "config.hpp"
#include "ec.hpp"
#include "sensors.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace cfc {

// What one fan thread needs for one control step. The sampler fills this in, so
// the fan thread never learns whether the numbers came from the CPU or the GPU.
struct FanInput {
    int temp = 0;           // C
    double avg_power = 0.0; // W, averaged over power_sample_count samples
    const FanParams* params = nullptr;
};

// Snapshot published by the sampler and read by the fan threads.
struct Snapshot {
    FanInput fans[2]; // index 0 = fan 1 (CPU), index 1 = fan 2 (GPU)
};

class Control {
public:
    Control(const Config& cfg, Ec& ec);

    // Opens the CPU sensors and locates the dGPU. Called on the main thread
    // before any worker starts; a false result is fatal for the caller.
    bool probe_sensors();

    // Samples at the configured interval, publishes a Snapshot, and re-reads
    // the dGPU state.
    void sampler_loop(std::stop_token stop);

    // Runs the speed law for one fan until stopped. fan is 1 or 2.
    void fan_loop(std::stop_token stop, int fan);

    Snapshot get() const;

private:
    const Config& cfg_;
    Ec& ec_;

    // Opened once by probe_sensors(); only the sampler thread reads them after.
    std::optional<RaplEnergy> rapl_;
    std::optional<CpuTemp> cpu_temp_;
    std::optional<std::string> dgpu_status_;

    mutable std::mutex mu_;
    Snapshot snap_;

    // Separate lock for the period wait: a fan thread must not hold the lock
    // that guards snap_ while it sleeps, or the sampler would block on it.
    std::mutex wait_mu_;
    std::condition_variable_any wait_cv_;

    // one averaging window per data source
    std::vector<double> cpu_hist_;
    std::vector<double> gpu_hist_;

    // last GPU reading, kept when a later query fails
    int last_gpu_temp_ = 0;
    double last_gpu_power_ = 0.0;
    bool have_gpu_ = false;

    // CPU readings, kept when a single read fails
    int last_cpu_temp_ = 0;

    bool logged_gpu_mode_ = false; // first mode report is emitted once
    bool prev_gpu_active_ = false;};

} // namespace cfc
