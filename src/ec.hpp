#pragma once

#include <mutex>

namespace cfc {

// Serializes every EC transaction. clevoec.c carries no lock of its own, and
// both fan threads plus the shutdown path reach it concurrently, so the whole
// read/write sequence must be held, not just one byte.
class Ec {
public:
    bool init();

    // fan is 1 (CPU) or 2 (GPU); percent is 0-100.
    int set_speed(int fan, int percent);
    int set_auto(int fan);
    int rpm(int fan);

private:
    std::mutex mu_;
};

} // namespace cfc
