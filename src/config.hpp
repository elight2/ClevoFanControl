#pragma once

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
    int power_sample_count = 0; // samples averaged for the min speed lookup
    std::vector<std::pair<int, int>> min_speed_list; // (power W, speed %), power ascending
};

struct Config {
    FanParams cpu;
    FanParams gpu;          // used while the dGPU is active
    FanParams gpu_cpu_only; // used while the dGPU is suspended

    bool debug_log = false;   // [general].log_level == "debug"
    int hw_detect_interval_ms = 1000; // [general].hw_detect_interval, in ms
};

// Validates every field and rejects the whole file on the first problem.
std::optional<Config> load_config(const std::string& path);

} // namespace cfc
