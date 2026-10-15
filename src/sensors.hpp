#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace cfc {

// Timeout for one nvidia-smi invocation. A hung driver would otherwise block
// the sampler forever, and the fans would stop being adjusted.
constexpr int kNvidiaSmiTimeoutMs = 5000;

// CPU package energy counter, in micro-joules. Wraps at max_energy_range_uj.
class RaplEnergy {
public:
    static std::optional<RaplEnergy> open(const std::string& dir);

    // Monotonic energy in uJ with wraparound folded in. Nullopt on read error.
    std::optional<uint64_t> read();

private:
    std::string energy_path_;
    uint64_t max_range_ = 0;
    uint64_t wraps_ = 0;
    uint64_t last_raw_ = 0;
    bool have_last_ = false;
};

// CPU package temperature, in degrees Celsius.
class CpuTemp {
public:
    static std::optional<CpuTemp> open();

    std::optional<int> read() const;

private:
    std::string input_path_;
};

// Path to the NVIDIA GPU's power/runtime_status, or nullopt if none found.
std::optional<std::string> find_nvidia_runtime_status();

// nvidia-smi query result: (temperature C, power W). Nullopt on any failure.
std::optional<std::pair<int, double>> nvidia_readings();

} // namespace cfc
