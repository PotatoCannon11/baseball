#include "platform/memory.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

namespace platform {

// UNTESTED on Windows: written from the documented PROCESS_MEMORY_COUNTERS_EX
// layout, not verified on real hardware. Verify via the windows-latest leg
// of .github/workflows/ci.yml (tests/platform_memory_test.cpp) before
// trusting these numbers, and report back once that CI run is available.
ProcessMemory get_process_memory() {
    ProcessMemory result;

    PROCESS_MEMORY_COUNTERS_EX counters;
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                               sizeof(counters))) {
        return result;
    }

    // PrivateUsage, not WorkingSetSize: the spec explicitly calls out that
    // working-set double-counts shared/mapped pages the way Linux RSS does.
    result.private_bytes = static_cast<std::uint64_t>(counters.PrivateUsage);
    result.resident_bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
    result.valid = true;
    return result;
}

}  // namespace platform

#endif  // _WIN32
