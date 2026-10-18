#include "control.hpp"

#include "log.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <numeric>

namespace cfc {
namespace {

// Monotonic nanoseconds. The energy counter advances with the same clock, so
// the two differences stay consistent; system_clock could step under NTP.
uint64_t monotonic_ns() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
           static_cast<uint64_t>(ts.tv_nsec);
}

using namespace std::chrono_literals;

// Startup speed. libclevoec has no way to ask the EC for its current duty, so the
// software must pick a value; 20% matches the old CfcDef::DEFAULT_SPEED.
constexpr int kInitialSpeed = 20;

// Step lookup from min_speed_list: the average power selects a row, and its
// speed becomes the floor. Below the first row the first speed applies, above
// the last row the last speed applies.
int lookup_min_speed(const std::vector<std::pair<int, int>>& list, double avg_power) {
    int speed = list.front().second;
    for (const auto& [power, s] : list) {
        if (avg_power >= power)
            speed = s;
        else
            break;
    }
    return speed;
}

// Average of the newest n samples. The history may be kept longer than one fan
// needs, because a single source can feed two fans with different window sizes:
// the CPU power history serves fan 1 through [cpu] and fan 2 through
// [gpu_cpu_only] while the dGPU is suspended.
double average_tail(const std::vector<double>& v, std::size_t n) {
    if (v.empty() || n == 0)
        return 0.0;
    const std::size_t take = std::min(n, v.size());
    const auto first = v.end() - static_cast<std::ptrdiff_t>(take);
    return std::accumulate(first, v.end(), 0.0) / static_cast<double>(take);
}

void push_window(std::vector<double>& v, double value, std::size_t cap) {
    v.push_back(value);
    if (v.size() > cap)
        v.erase(v.begin());
}

// Reads power/runtime_status. A failed read is not "suspended": it is unknown,
// and reporting it as suspended would flip the fan mode on a transient error.
std::optional<bool> read_gpu_active(const std::string& status_path) {
    std::ifstream f(status_path);
    std::string status;
    if (!f || !std::getline(f, status))
        return std::nullopt;
    return status == "active";
}

} // namespace

Control::Control(const Config& cfg, Ec& ec) : cfg_(cfg), ec_(ec) {
    snap_.fans[0].params = &cfg_.cpu;
    snap_.fans[1].params = &cfg_.gpu_cpu_only;
}

bool Control::probe_sensors() {
    // The RAPL domain path is hardcoded to the first package domain.
    // AMD support is deliberately not claimed. The intel_rapl_msr driver does
    // register its control type as "intel-rapl" on AMD and Hygon as well, and
    // intel_rapl_common.c binds it to AMD families 0x17, 0x19, 0x1A and Hygon
    // 0x18, so this open can succeed there. That is not enough: CpuTemp only
    // accepts coretemp (see sensors.cpp), so an AMD machine still stops at the
    // temperature probe below. Neither path was tested on AMD hardware.
    rapl_ = RaplEnergy::open("/sys/class/powercap/intel-rapl:0");
    if (!rapl_) {
        error("cannot open /sys/class/powercap/intel-rapl:0 (need root, or no RAPL "
              "package domain on this CPU)");
        return false;
    }

    cpu_temp_ = CpuTemp::open();
    if (!cpu_temp_) {
        error("no coretemp hwmon device with temp1_input found; "
              "this build only supports Intel CPUs");
        return false;
    }

    dgpu_status_ = find_nvidia_runtime_status();
    if (dgpu_status_)
        logf("dGPU detected: {}", *dgpu_status_);
    else
        log("no NVIDIA dGPU found; GPU fan stays in CPU-only mode");

    return true;
}

void Control::sampler_loop(std::stop_token stop) {
    // Window sizes come from the table each fan actually uses: fan 1 reads
    // [cpu], fan 2 reads [gpu] while the dGPU is active and [gpu_cpu_only] while
    // it is suspended. In CPU-only mode fan 2 is driven by the CPU power, so the
    // CPU history must be kept long enough for both [cpu] and [gpu_cpu_only];
    // each fan then averages the newest count its own table asks for.
    const auto cpu_window = static_cast<std::size_t>(cfg_.cpu.power_sample_count);
    const auto cpu_only_window = static_cast<std::size_t>(cfg_.gpu_cpu_only.power_sample_count);
    const auto cpu_hist_cap = std::max(cpu_window, cpu_only_window);
    const auto gpu_window = static_cast<std::size_t>(cfg_.gpu.power_sample_count);

    // First read establishes the baseline. The first sample taken inside the
    // loop only refreshes that baseline; it must not enter the averaging window.
    auto last_energy = rapl_->read();
    uint64_t last_ns = monotonic_ns();
    bool baseline_pending = true;

    while (!stop.stop_requested()) {
        // Plain sleep_for, not a condition_variable wait: shutdown can therefore
        // be delayed by up to one hw_detect_interval. Acceptable for a periodic
        // sampler, but do not assume the loop reacts to stop immediately.
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.hw_detect_interval_ms));
        if (stop.stop_requested())
            break;

        const uint64_t now_ns = monotonic_ns();

        if (baseline_pending) {
            // Baseline only: no power sample, no window entry, no publish.
            if (auto energy = rapl_->read())
                last_energy = energy;
            last_ns = now_ns;
            baseline_pending = false;
            continue;
        }

        double cpu_power = 0.0;
        if (auto energy = rapl_->read()) {
            if (last_energy) {
                double delta_uj = static_cast<double>(*energy - *last_energy);
                double delta_ms = static_cast<double>(now_ns - last_ns) / 1e6;
                if (delta_ms > 0.0)
                    cpu_power = delta_uj / delta_ms / 1000.0; // uJ/ms = mW, then W
            }
            last_energy = energy;
        }
        last_ns = now_ns;

        int cpu_temp_now = last_cpu_temp_;
        if (auto t = cpu_temp_->read()) {
            last_cpu_temp_ = *t; // a failed read keeps the previous value
            cpu_temp_now = *t;
        }
        push_window(cpu_hist_, cpu_power, cpu_hist_cap);

        // Mode is derived from power/runtime_status, a free sysfs read that does
        // not wake the GPU. It stays the authority: a failed read is unknown, not
        // "suspended", and is treated as GPU+CPU so a transient error cannot flip
        // the fan mode.
        bool used_real_gpu = false;
        bool fresh_reading = false;
        // Set when either nvidia-smi call timed out this cycle. Reported once per
        // state change, not once per cycle, so a driver that stays stuck for
        // minutes does not fill the journal.
        bool timeout_now = false;

        if (dgpu_status_) {
            const bool active = read_gpu_active(*dgpu_status_).value_or(true);
            const bool pausing = pause_until_ns_.has_value();

            if (!active) {
                // Suspended: any pause is over, and a fresh active phase must be
                // able to judge its own conditions from scratch.
                pause_until_ns_.reset();
            }

            if (active && !pausing) {
                // nvidia-smi wakes the GPU, so it runs only when the sysfs state
                // says the GPU is already awake and no pause is in effect.
                const auto [st, r] = nvidia_readings();
                if (st == QueryStatus::Ok && r) {
                    last_gpu_temp_ = r->first;
                    last_gpu_power_ = r->second;
                    have_gpu_ = true;
                    fresh_reading = true;
                } else if (st == QueryStatus::Timeout) {
                    // A timeout is a late reading, not a broken one: keep using the
                    // last good values as this cycle's reading. They enter the
                    // averaging window like any other sample, so the control law
                    // keeps steering on the most recent real measurement.
                    timeout_now = true;
                    if (have_gpu_)
                        fresh_reading = true;
                } else {
                    debug("nvidia-smi query failed; keeping last reading");
                }
                // A failed query keeps the previous reading; with no reading at
                // all (first query already failed) the CPU-only table takes over.
                used_real_gpu = have_gpu_;

                // A hot GPU cannot be about to suspend, so the second nvidia-smi
                // call is not worth the extra wake-up.
                if (used_real_gpu && cfg_.gpu_detect.enabled &&
                    last_gpu_temp_ < cfg_.gpu_detect.sleep_temp) {
                    debug("querying GPU process list");
                    const auto [pst, procs] = nvidia_processes(cfg_.gpu_detect.proc_exclude);
                    if (pst == QueryStatus::Timeout) {
                        // No answer about the client set: do not pause on a guess.
                        // The query is retried on the next cycle, so a single slow
                        // nvidia-smi costs a cycle, not the sleep feature.
                        timeout_now = true;
                    } else if (!procs) {
                        // Unknown client set: keep polling so the fans see fresh
                        // data rather than a frozen reading.
                        debug("nvidia-smi -q -d PIDS failed; keeping GPU polling");
                    } else if (procs->unidentified) {
                        // Holders the driver cannot name: treat as in use.
                        debug("nvidia-smi reports unidentified GPU processes");
                    } else if (!procs->in_use) {
                        pause_until_ns_ =
                            now_ns + static_cast<uint64_t>(cfg_.gpu_detect.delay_ms) * 1000000ULL;
                        logf("GPU has no client processes: pausing nvidia-smi for {} s",
                             cfg_.gpu_detect.delay_ms / 1000);
                    }
                }
            } else if (active) {
                // Pause in effect: no nvidia-smi at all, so the driver gets an
                // uninterrupted idle window. Keep serving the last reading.
                used_real_gpu = have_gpu_;
                if (now_ns >= *pause_until_ns_) {
                    pause_until_ns_.reset();
                    logf("GPU still active after {} s: resuming detection",
                         cfg_.gpu_detect.delay_ms / 1000);
                }
            }
        }

        // While paused the averaging window is frozen: the value is not a new
        // measurement, and pushing it would slowly rewrite the history.
        if (used_real_gpu && fresh_reading)
            push_window(gpu_hist_, last_gpu_power_, gpu_window);
        else if (!used_real_gpu)
            gpu_hist_.clear(); // stale history must not leak into the next active phase

        // Report the first decision, and every later transition between the two
        // modes. Running nvidia-smi wakes the dGPU, so these are worth seeing.
        if (!logged_gpu_mode_ || used_real_gpu != prev_gpu_active_) {
            log(used_real_gpu ? "GPU fan mode: GPU+CPU (dGPU active)"
                              : "GPU fan mode: CPU-only (dGPU suspended)");
            logged_gpu_mode_ = true;
            prev_gpu_active_ = used_real_gpu;
        }

        // One normal-level line per state change, in both directions. Emitting on
        // every cycle would flood the journal while the driver stays hung. The
        // two timeouts share the message because the user-visible effect is the
        // same: this cycle ran without fresh GPU data.
        if (timeout_now != prev_timeout_) {
            if (timeout_now)
                errorf("nvidia-smi exceeded the {} s timeout; GPU data for this cycle "
                       "falls back to the last good reading and GPU sleep detection is "
                       "skipped, retrying next cycle",
                       kNvidiaSmiTimeoutMs / 1000);
            else
                log("nvidia-smi responding again");
            prev_timeout_ = timeout_now;
        }

        Snapshot next;
        next.fans[0].temp = cpu_temp_now;
        next.fans[0].avg_power = average_tail(cpu_hist_, cpu_window);
        next.fans[0].params = &cfg_.cpu;

        if (used_real_gpu) {
            next.fans[1].temp = last_gpu_temp_;
            next.fans[1].avg_power = average_tail(gpu_hist_, gpu_window);
            next.fans[1].params = &cfg_.gpu;
        } else {
            // CPU-only mode: fan 2 borrows the CPU temperature and power but is
            // tuned by [gpu_cpu_only], including that table's sample count.
            next.fans[1].temp = cpu_temp_now;
            next.fans[1].avg_power = average_tail(cpu_hist_, cpu_only_window);
            next.fans[1].params = &cfg_.gpu_cpu_only;
        }

        {
            std::lock_guard lock(mu_);
            snap_ = next;
        }
    }
}

void Control::fan_loop(std::stop_token stop, int fan) {
    const int index = fan - 1;
    int cur_speed = kInitialSpeed;

    if (ec_.set_speed(fan, cur_speed) != 0)
        return;

    // Note: set_speed() cannot fail in practice (libclevoec returns no status for
    // a write), so this early return only triggers on an argument error. A fan
    // thread that dies here leaves its fan in whatever the EC had, while the
    // process keeps running: the main loop only watches for signals.

    while (!stop.stop_requested()) {
        FanInput in;
        int interval_ms = 0;
        {
            std::unique_lock lock(mu_);
            // Params pointers refer to cfg_, which lives for the whole process,
            // so they stay valid after the lock is released.
            in = snap_.fans[index];
            interval_ms = in.params->interval_ms;
        }

        const FanParams& p = *in.params;
        const int min_speed = lookup_min_speed(p.min_speed_list, in.avg_power);

        if (cur_speed < min_speed) {
            // Rule 1: never sit below the current allowed floor. One step per
            // cycle, and this cycle does nothing else.
            const int next = std::min(cur_speed + p.step, min_speed);
            debugf("fan {}: {} -> {} min-driven (temp {}, pwr {:.1f}, min {})",
                   fan, cur_speed, next, in.temp, in.avg_power, min_speed);
            cur_speed = next;
            ec_.set_speed(fan, cur_speed);
        } else {
            // Rule 2: move toward the target temperature band.
            int target = cur_speed;
            if (in.temp > p.target_temp + p.temp_offset)
                target = cur_speed + p.step;
            else if (in.temp < p.target_temp - p.temp_offset)
                target = cur_speed - p.step;
            target = std::clamp(target, min_speed, 100);

            if (target != cur_speed) {
                // Only report an actual change; a steady state stays silent.
                const int delta = target - cur_speed;
                debugf("fan {}: {} -> {} {}{} temp-driven (temp {}, pwr {:.1f}, min {})",
                       fan, cur_speed, target, delta > 0 ? "+" : "-",
                       std::abs(delta), in.temp, in.avg_power, min_speed);
                cur_speed = target;
                ec_.set_speed(fan, cur_speed);
            }
        }

        // The stop_token overload wakes immediately on shutdown; the predicate
        // stays false so the wait always ends by timeout otherwise.
        std::unique_lock lock(wait_mu_);
        wait_cv_.wait_for(lock, stop, std::chrono::milliseconds(interval_ms),
                          [] { return false; });
    }
}

} // namespace cfc
