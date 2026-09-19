#pragma once

#include <cstdint>

// Process memory query. Every OS reports a different notion of "how much
// memory is this process using"; the spec calls for the one that tracks
// pages this process actually owns (Windows calls it "private bytes",
// distinct from "working set" which double-counts shared/mapped pages).
//
// Linux:   Private_Clean + Private_Dirty from /proc/self/smaps_rollup.
//          Falls back to VmRSS from /proc/self/status if smaps_rollup is
//          unavailable (older kernels: rollup landed in Linux 4.14), and
//          documents that fallback as a rough over-estimate (RSS includes
//          shared pages, e.g. mapped shared libraries).
// macOS:   task_info(TASK_VM_INFO) -> phys_footprint. This is what Activity
//          Monitor calls "Memory" and Apple documents as the closest analog
//          to Windows private bytes (excludes most shared/mapped-file pages).
//          UNTESTED on macOS as of writing -- verify in CI (see
//          .github/workflows/ci.yml) before trusting the number.
// Windows: GetProcessMemoryInfo(PROCESS_MEMORY_COUNTERS_EX) -> PrivateUsage.
//          UNTESTED on Windows as of writing -- verify in CI.
namespace platform {

struct ProcessMemory {
    // Best available "private bytes" figure: pages owned exclusively by
    // this process, not counting shared mappings. See file header for the
    // per-OS source of this number.
    std::uint64_t private_bytes = 0;
    // Resident set size (all pages currently in RAM, including shared).
    // Always >= private_bytes. Reported for cross-checking.
    std::uint64_t resident_bytes = 0;
    // False if the query failed (unsupported kernel, permission, etc.);
    // fields are then 0 and must not be treated as a real measurement.
    bool valid = false;
};

// Queries current process memory. Safe to call every frame for the debug
// overlay: on Linux it does a couple of small file reads, no heap
// allocation of consequence, and no syscall exotica.
ProcessMemory get_process_memory();

}  // namespace platform
