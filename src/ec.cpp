#include "ec.hpp"

#include "log.hpp"

extern "C" {
#include "libclevoec.h"
}

#include <cerrno>
#include <cstring>

namespace cfc {
namespace {

// libclevoec speaks in EC RAM addresses, not fan numbers, because the tachometer
// address is the only fan-specific part of a RPM read. 0 means "unknown fan".
unsigned char rpm_addr(int fan) {
    switch (fan) {
    case 1:
        return EC_RPM1;
    case 2:
        return EC_RPM2;
    default:
        return 0;
    }
}

} // namespace

bool Ec::init() {
    // init_perm() returns -1 (not -errno) and leaves the reason in errno.
    if (init_perm() != 0) {
        logf("init_perm failed: {}", std::strerror(errno));
        return false;
    }
    return true;
}

int Ec::set_speed(int fan, int percent) {
    if (fan != 1 && fan != 2) {
        logf("refusing unknown fan {}", fan);
        return -1;
    }
    if (percent < 0 || percent > 100) {
        logf("refusing fan {} speed {}: out of range", fan, percent);
        return -1;
    }
    // libclevoec takes the raw 0-255 duty cycle
    int duty = percent * 255 / 100;

    // libclevoec reports no status for a write: fix_fan_duty() returns void, so a
    // failed transaction cannot be detected here. The EC command path has no
    // error response either, so returning success is the only option.
    std::lock_guard lock(mu_);
    fix_fan_duty(static_cast<unsigned char>(fan), static_cast<unsigned char>(duty));
    return 0;
}

int Ec::set_auto(int fan) {
    if (fan != 1 && fan != 2) {
        logf("refusing unknown fan {}", fan);
        return -1;
    }

    // Same as set_speed: release_fan_duty() returns void, so no status to report.
    std::lock_guard lock(mu_);
    release_fan_duty(static_cast<unsigned char>(fan));
    return 0;
}

int Ec::rpm(int fan) {
    // Note: get_rpm() computes 2156250 / raw, while every reference document in
    // .cache/ec and the previous implementation use 2156220 / raw. The 0.0014%
    // difference is below what a tachometer resolves, so it is not touched here,
    // but the constant has no documented source in libclevoec.c.
    const unsigned char addr = rpm_addr(fan);
    if (addr == 0) {
        logf("refusing RPM read for unknown fan {}", fan);
        return -1;
    }

    std::lock_guard lock(mu_);
    return static_cast<int>(get_rpm(addr));
}

} // namespace cfc
