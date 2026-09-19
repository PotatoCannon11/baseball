#include "platform/memory.h"

#if defined(__linux__)

#include <cstdio>
#include <cstring>

namespace platform {

namespace {

// Parses "Private_Clean:   1234 kB" style lines from /proc/self/smaps_rollup.
// Returns true and adds to *out_kb if the prefix matches.
bool add_if_prefixed(const char* line, const char* prefix, std::uint64_t* out_kb) {
    const std::size_t prefix_len = std::strlen(prefix);
    if (std::strncmp(line, prefix, prefix_len) != 0) {
        return false;
    }
    std::uint64_t kb = 0;
    if (std::sscanf(line + prefix_len, "%lu", &kb) == 1) {
        *out_kb += kb;
    }
    return true;
}

bool read_smaps_rollup_private_kb(std::uint64_t* out_kb) {
    std::FILE* f = std::fopen("/proc/self/smaps_rollup", "r");
    if (!f) {
        return false;
    }
    char line[256];
    std::uint64_t total_kb = 0;
    bool found_any = false;
    while (std::fgets(line, sizeof(line), f)) {
        if (add_if_prefixed(line, "Private_Clean:", &total_kb)) {
            found_any = true;
        } else if (add_if_prefixed(line, "Private_Dirty:", &total_kb)) {
            found_any = true;
        }
    }
    std::fclose(f);
    if (!found_any) {
        return false;
    }
    *out_kb = total_kb;
    return true;
}

bool read_status_vmrss_kb(std::uint64_t* out_kb) {
    std::FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) {
        return false;
    }
    char line[256];
    bool found = false;
    std::uint64_t kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmRSS:", 6) == 0) {
            if (std::sscanf(line + 6, "%lu", &kb) == 1) {
                found = true;
            }
            break;
        }
    }
    std::fclose(f);
    if (!found) {
        return false;
    }
    *out_kb = kb;
    return true;
}

}  // namespace

ProcessMemory get_process_memory() {
    ProcessMemory result;

    std::uint64_t rss_kb = 0;
    result.valid = read_status_vmrss_kb(&rss_kb);
    result.resident_bytes = rss_kb * 1024ull;

    std::uint64_t private_kb = 0;
    if (read_smaps_rollup_private_kb(&private_kb)) {
        result.private_bytes = private_kb * 1024ull;
        result.valid = true;
    } else if (result.valid) {
        // smaps_rollup unavailable (pre-4.14 kernel or restricted): fall
        // back to RSS. This over-counts shared pages (e.g. mapped libc),
        // so treat it as an upper bound, not the real private-bytes figure.
        result.private_bytes = result.resident_bytes;
    }

    return result;
}

}  // namespace platform

#endif  // __linux__
