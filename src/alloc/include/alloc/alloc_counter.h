#pragma once

#include <cstdint>

// Global heap-allocation counter. Overrides the global operator new/delete
// family (see src/alloc_counter.cpp) so ANY heap allocation in the process
// -- ours or a library's, as long as it goes through the global operators
// rather than a custom allocator -- is counted here.
//
// This is how the "zero allocations during a 60 s simulated run" test
// (milestone 3+) will be enforced: snapshot get_stats() before and after a
// scope, and assert the allocation count did not change.
//
// Known gap: over-aligned allocations (C++17 operator new(size_t,
// align_val_t), used for types with alignment > alignof(std::max_align_t),
// e.g. some SIMD vector types) are NOT counted, because we only override
// the non-aligned new/delete overloads. Fine for milestone 1; revisit if
// the sim ever uses over-aligned types.
namespace alloc {

struct AllocStats {
    std::uint64_t total_allocations = 0;  // count of operator new calls
    std::uint64_t total_frees = 0;        // count of operator delete calls
    std::uint64_t bytes_allocated = 0;    // sum of requested sizes
    std::uint64_t bytes_freed = 0;        // sum of freed sizes
};

// Snapshot of the counters right now. Cheap (a few atomic loads); safe to
// call every frame.
AllocStats get_stats();

// Debug aid: the next `count` allocations after this call print a
// backtrace to stderr, then logging turns itself off. For tracking down
// "where did this allocation actually come from" (e.g. a third-party
// library's own internal `new` -- overriding global operator new/delete
// catches those too, not just this codebase's own allocations, which is
// exactly what makes this useful for telling the two apart). Not for
// shipping, just development. No-op on platforms without <execinfo.h>
// (Windows).
void debug_log_next_allocations(int count);

}  // namespace alloc
