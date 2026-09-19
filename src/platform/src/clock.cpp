#include "platform/clock.h"

#include <chrono>

namespace platform {

std::uint64_t monotonic_now_ns() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

double monotonic_now_seconds() {
    return static_cast<double>(monotonic_now_ns()) * 1e-9;
}

}  // namespace platform
