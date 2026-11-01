#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cfc {

// Timeout for one nvidia-smi invocation. A hung driver would otherwise block
// the sampler forever, and the fans would stop being adjusted.
constexpr int kNvidiaSmiTimeoutMs = 5000;

// How an nvidia-smi query ended. Callers must tell a timeout apart from any
// other failure: a timeout means the reading is merely late and the previous
// one stays usable, while a plain failure is a separate case with its own
// handling.
enum class QueryStatus { Ok, Timeout, Failed };

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

// CPU package temperature, in degrees Celsius. Intel coretemp only; AMD support
// is deliberately not claimed, see the implementation.
class CpuTemp {
public:
    static std::optional<CpuTemp> open();

    std::optional<int> read() const;

private:
    std::string input_path_;
};

// Path to the NVIDIA GPU's power/runtime_status, or nullopt if none found.
std::optional<std::string> find_nvidia_runtime_status();

// nvidia-smi query result: (temperature C, power W). status tells a timeout
// apart from other failures; the value is only meaningful when status is Ok.
std::pair<QueryStatus, std::optional<std::pair<int, double>>> nvidia_readings();

// GPU clients as reported by `nvidia-smi -q -d PIDS`. This is the driver's own
// accounting, not the set of open /dev/nvidia* handles: a display server keeps
// those open even while the GPU is permitted to suspend.
struct GpuProcList {
    std::vector<std::string> names;
    bool unidentified = false; // holders exist but the driver cannot name them
    bool in_use = false;       // some client survives the exclusion list
};

// status is Failed when the query failed or its output had no Processes section.
std::pair<QueryStatus, std::optional<GpuProcList>>
nvidia_processes(const std::vector<std::string>& exclude);

} // namespace cfc
