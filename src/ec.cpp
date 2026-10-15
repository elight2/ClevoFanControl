#include "ec.hpp"

#include "log.hpp"

extern "C" {
#include "clevoec.h"
}

#include <cstring>

namespace cfc {

bool Ec::init() {
    int rc = ec_init();
    if (rc != 0) {
        logf("ec_init failed: {}", std::strerror(-rc));
        return false;
    }
    return true;
}

int Ec::set_speed(int fan, int percent) {
    if (percent < 0 || percent > 100) {
        logf("refusing fan {} speed {}: out of range", fan, percent);
        return -1;
    }
    // clevoec takes the raw 0-255 duty cycle
    int duty = percent * 255 / 100;

    std::lock_guard lock(mu_);
    int rc = set_fan_speed(duty, fan);
    if (rc != 0)
        logf("set_fan_speed(fan {}, duty {}) failed: {}", fan, duty, rc);
    return rc;
}

int Ec::set_auto(int fan) {
    std::lock_guard lock(mu_);
    int rc = set_fan_speed(-1, fan);
    if (rc != 0)
        logf("set_fan_speed(fan {}, auto) failed: {}", fan, rc);
    return rc;
}

int Ec::rpm(int fan) {
    std::lock_guard lock(mu_);
    return get_rpm(fan);
}

} // namespace cfc
