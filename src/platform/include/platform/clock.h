#pragma once

#include <cstdint>

// Thin wrapper over a monotonic high-resolution clock. Implemented today in
// terms of std::chrono::steady_clock, which is already portable across GCC,
// Clang, and MSVC -- the wrapper exists so every timing read in the codebase
// goes through one call site, so it can be swapped later (e.g. for a
// TSC-based clock) without touching callers, and so it stays out of the sim
// library (the sim must never read a clock itself; only the input/frame
// layers may call this).
namespace platform {

// Nanoseconds since an unspecified but fixed epoch (process start is a
// common choice; do not assume it matches wall-clock time).
std::uint64_t monotonic_now_ns();

// Convenience seconds view of the same clock, as a double.
double monotonic_now_seconds();

}  // namespace platform
