#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cfc {

// One fan's tuning table. Immutable once loaded, so threads may share a pointer to it.
struct FanParams {
    int target_temp = 0;        // C
    int temp_offset = 0;        // C, half-width of the dead band
    int step = 0;               // percent per adjustment
    int interval_ms = 0;        // control period, already converted from seconds
    // Samples averaged for the min speed lookup.
    // KNOWN DEFECT (confirmed by the project owner, fix pending): the sampler
    // builds both averaging windows from cfg_.cpu.power_sample_count only, so
    // the values read from [gpu] and [gpu_cpu_only] are parsed and validated but
    // never used. The old Qt implementation kept one window per fan.
    int power_sample_count = 0;
    std::vector<std::pair<int, int>> min_speed_list; // (power W, speed %), power ascending
};

// dGPU sleep detection, from the [gpu_detect] table.
struct GpuDetect {
    bool enabled = false;                  // gpu_sleep_detect_enabled
    int64_t delay_ms = 30000;              // gpu_sleep_delay, seconds in the file
    int sleep_temp = 60;                   // gpu_sleep_temp, C
    std::vector<std::string> proc_exclude; // gpu_proc_exclude, substring match
};

struct Config {
    FanParams cpu;
    FanParams gpu;          // used while the dGPU is active
    FanParams gpu_cpu_only; // used while the dGPU is suspended

    bool debug_log = false;   // [general].log_level == "debug"
    int hw_detect_interval_ms = 1000; // [general].hw_detect_interval, in ms

    GpuDetect gpu_detect;
};

// Validates every field and rejects the whole file on the first problem.
std::optional<Config> load_config(const std::string& path);

} // namespace cfc
