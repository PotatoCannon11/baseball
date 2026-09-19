#include "platform/memory.h"

#if defined(__APPLE__)

#include <mach/mach.h>
#include <mach/task.h>
#include <mach/task_info.h>

namespace platform {

// UNTESTED on macOS: written from Apple's documented TASK_VM_INFO struct
// layout, not verified on real hardware. Verify via the macOS leg of
// .github/workflows/ci.yml (tests/platform_memory_test.cpp) before trusting
// these numbers, and report back once that CI run is available.
ProcessMemory get_process_memory() {
    ProcessMemory result;

    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_VM_INFO,
                                  reinterpret_cast<task_info_t>(&info), &count);
    if (kr != KERN_SUCCESS) {
        return result;
    }

    // phys_footprint is what Activity Monitor labels "Memory" and is Apple's
    // documented closest analog to Windows' PrivateUsage: it excludes most
    // shared/mapped-file pages that resident_size would double-count.
    result.private_bytes = info.phys_footprint;
    result.resident_bytes = info.resident_size;
    result.valid = true;
    return result;
}

}  // namespace platform

#endif  // __APPLE__
