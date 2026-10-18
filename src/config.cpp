#include "config.hpp"

#include "log.hpp"

#include <toml++/toml.hpp>

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace cfc {
namespace {

constexpr int kMinIntervalMs = 100; // below this the EC is driven too fast
constexpr double kMinIntervalSec = kMinIntervalMs / 1000.0;

// Default configuration, embedded at build time from src/config.toml so no
// template file has to be shipped next to the binary. #embed resolves the path
// relative to this source file, and the compiler records config.toml as a
// dependency, so editing the template triggers a rebuild. Note: #embed is only
// standard from C++26; GCC accepts it in C++23 as an extension, which is why
// this needs GCC >= 15 or Clang >= 19.
// Note: sizeof(kDefaultConfig) is the file size; #embed does not append a
// terminator, so it is written as raw bytes rather than as a C string.
constexpr unsigned char kDefaultConfig[] = {
#embed "config.toml"
};

// Creates path and writes the embedded default into it.
// Returns false on a failed open or a short write; errno carries the reason for
// the open case, which is the one a caller can act on (a read-only directory).
bool write_default_config(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char*>(kDefaultConfig),
              static_cast<std::streamsize>(sizeof(kDefaultConfig)));
    return static_cast<bool>(out);
}

// The sampler wakes the dGPU by running nvidia-smi, so its period has a higher
// floor than the fan control period.
constexpr int kMinDetectIntervalMs = 500;
constexpr double kMinDetectIntervalSec = kMinDetectIntervalMs / 1000.0;

std::optional<FanParams> parse_fan(const toml::table& t, std::string_view name) {
    FanParams p;

    auto fail = [&](std::string_view why) {
        logf("[{}] {}", name, why);
        return std::optional<FanParams>{};
    };

    // scalar fields
    auto get_int = [&](std::string_view key) -> std::optional<int64_t> {
        auto v = t[key].value<int64_t>();
        if (!v)
            logf("[{}] missing or non-integer key '{}'", name, key);
        return v;
    };

    auto target = get_int("target_temp");
    auto offset = get_int("target_temp_offset");
    auto step = get_int("ctrl_step");
    auto count = get_int("power_sample_count");
    if (!target || !offset || !step || !count)
        return std::nullopt;

    auto interval = t["ctrl_interval"].value<double>();
    if (!interval) {
        logf("[{}] missing or non-float key 'ctrl_interval'", name);
        return std::nullopt;
    }

    // min_speed_list: array of [power W, speed %] pairs
    auto* list = t["min_speed_list"].as_array();
    if (!list || list->empty())
        return fail("'min_speed_list' must be a non-empty array");

    p.min_speed_list.reserve(list->size());
    for (const auto& node : *list) {
        auto* pair = node.as_array();
        if (!pair || pair->size() != 2)
            return fail("each 'min_speed_list' entry must be a [power, speed] pair");
        auto power = (*pair)[0].value<int64_t>();
        auto speed = (*pair)[1].value<int64_t>();
        if (!power || !speed)
            return fail("'min_speed_list' entries must be integers");
        p.min_speed_list.emplace_back(static_cast<int>(*power), static_cast<int>(*speed));
    }
    // Note: 'power' has a lower bound but no upper bound, so a value above
    // INT_MAX is narrowed to int here. Unrealistic for watts; left as-is rather
    // than adding a limit nobody asked for.

    // power must be strictly ascending, otherwise the step lookup is ambiguous
    for (std::size_t i = 1; i < p.min_speed_list.size(); i++) {
        if (p.min_speed_list[i].first <= p.min_speed_list[i - 1].first)
            return fail("'min_speed_list' power values must be strictly ascending");
    }
    for (const auto& [power, speed] : p.min_speed_list) {
        if (power < 0)
            return fail("'min_speed_list' power values must be >= 0");
        if (speed < 0 || speed > 100)
            return fail("'min_speed_list' speed values must be within 0-100");
    }

    if (*count < 1)
        return fail("'power_sample_count' must be >= 1");
    if (*target < 0 || *target > 150)
        return fail("'target_temp' must be within 0-150 C");
    if (*offset < 0)
        return fail("'target_temp_offset' must be >= 0");
    if (*step < 1)
        return fail("'ctrl_step' must be >= 1");
    if (!std::isfinite(*interval) || *interval < kMinIntervalSec)
        return fail(std::format("'ctrl_interval' must be >= {} s", kMinIntervalSec));

    // No upper bound is enforced: llround(seconds * 1000) above INT_MAX wraps on
    // the cast to int, which is undefined behaviour and would turn into a
    // nonsense (possibly negative) period. Unreachable with a config file a human
    // wrote; recorded rather than fixed because the limit is a policy choice.

    p.target_temp = static_cast<int>(*target);
    p.temp_offset = static_cast<int>(*offset);
    p.step = static_cast<int>(*step);
    p.power_sample_count = static_cast<int>(*count);
    p.interval_ms = static_cast<int>(std::llround(*interval * 1000.0));
    return p;
}

} // namespace

std::optional<Config> load_config(const std::string& path) {
    // First run: no config on disk yet, so materialize the embedded default.
    // Only a missing file triggers this. A file that exists but fails to parse
    // is reported as an error instead of being overwritten, so a config the user
    // broke is never silently replaced by the defaults.
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) && !ec) {
        if (!write_default_config(path)) {
            logf("no config at {} and cannot create it: {}", path, std::strerror(errno));
            return std::nullopt;
        }
        logf("no config at {}, wrote the built-in default", path);
    }

    toml::table root;
    try {
        root = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        logf("cannot parse {}: {}", path, std::string_view{e.description()});
        return std::nullopt;
    }

    Config cfg;
    bool ok = true;

    auto load_table = [&](std::string_view key, FanParams& out) {
        auto* table = root[key].as_table();
        if (!table) {
            logf("missing table [{}]", key);
            ok = false;
            return;
        }
        auto parsed = parse_fan(*table, key);
        if (parsed)
            out = std::move(*parsed);
        else
            ok = false;
    };

    load_table("cpu", cfg.cpu);
    load_table("gpu", cfg.gpu);
    load_table("gpu_cpu_only", cfg.gpu_cpu_only);

    auto* general = root["general"].as_table();
    if (!general) {
        log("missing table [general]");
        ok = false;
        return std::nullopt;
    }

    if (auto lv = (*general)["log_level"].value<std::string>()) {
        if (*lv == "normal")
            cfg.debug_log = false;
        else if (*lv == "debug")
            cfg.debug_log = true;
        else {
            logf("[general] 'log_level' must be \"normal\" or \"debug\", got \"{}\"", *lv);
            ok = false;
        }
    } else {
        log("[general] missing or non-string key 'log_level'");
        ok = false;
    }

    if (auto iv = (*general)["hw_detect_interval"].value<double>()) {
        if (!std::isfinite(*iv) || *iv < kMinDetectIntervalSec) {
            logf("[general] 'hw_detect_interval' must be >= {} s", kMinDetectIntervalSec);
            ok = false;
        } else {
            // Same missing upper bound as 'ctrl_interval': the cast to int can
            // overflow for an absurd interval.
            cfg.hw_detect_interval_ms = static_cast<int>(std::llround(*iv * 1000.0));
        }
    } else {
        log("[general] missing or non-float key 'hw_detect_interval'");
        ok = false;
    }

    auto* detect = root["gpu_detect"].as_table();
    if (!detect) {
        log("missing table [gpu_detect]");
        ok = false;
        return std::nullopt;
    }

    // toml++ converts integers to bool (0/1 and, surprisingly, 2), so inspect
    // the node type instead of trusting value<bool>().
    auto enabled_node = (*detect)["gpu_sleep_detect_enabled"];
    if (auto ev = enabled_node.value<bool>(); ev && enabled_node.is_boolean()) {
        cfg.gpu_detect.enabled = *ev;
    } else {
        log("[gpu_detect] missing or non-boolean key 'gpu_sleep_detect_enabled'");
        ok = false;
    }

    if (auto dv = (*detect)["gpu_sleep_delay"].value<int64_t>()) {
        if (*dv < 1) {
            log("[gpu_detect] 'gpu_sleep_delay' must be >= 1 s");
            ok = false;
        } else {
            cfg.gpu_detect.delay_ms = *dv * 1000;
        }
    } else {
        log("[gpu_detect] missing or non-integer key 'gpu_sleep_delay'");
        ok = false;
    }

    if (auto tv = (*detect)["gpu_sleep_temp"].value<int64_t>()) {
        if (*tv < 0) {
            log("[gpu_detect] 'gpu_sleep_temp' must be >= 0 C");
            ok = false;
        } else {
            cfg.gpu_detect.sleep_temp = static_cast<int>(*tv);
        }
    } else {
        log("[gpu_detect] missing or non-integer key 'gpu_sleep_temp'");
        ok = false;
    }

    auto* exclude = (*detect)["gpu_proc_exclude"].as_array();
    if (!exclude) {
        log("[gpu_detect] missing or non-array key 'gpu_proc_exclude'");
        ok = false;
    } else {
        cfg.gpu_detect.proc_exclude.reserve(exclude->size());
        for (const auto& node : *exclude) {
            auto s = node.value<std::string>();
            if (!s) {
                log("[gpu_detect] 'gpu_proc_exclude' entries must be strings");
                ok = false;
                break;
            }
            cfg.gpu_detect.proc_exclude.push_back(*s);
        }
    }

    if (!ok)
        return std::nullopt;
    return cfg;
}

} // namespace cfc
