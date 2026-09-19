#include "platform/clock.h"
#include "platform/memory.h"
#include "platform/paths.h"

#include <cstdio>

// Milestone 1 check: the platform layer's memory query, clock, and path
// helpers all work on whatever OS this test runs on. This is also the
// mechanism for reporting real process-memory numbers per platform, since
// CI actually executes this on linux/macos/windows runners (see
// .github/workflows/ci.yml) -- the printed numbers are real measurements,
// not guesses, on every OS the workflow runs on.
int main() {
    const platform::ProcessMemory mem = platform::get_process_memory();
    if (!mem.valid) {
        std::fprintf(stderr, "FAIL: platform::get_process_memory() reported invalid on this platform\n");
        return 1;
    }
    if (mem.private_bytes == 0) {
        std::fprintf(stderr, "FAIL: private_bytes was 0, that can't be right for a running process\n");
        return 1;
    }
    std::printf("process memory: private=%.2f MB resident=%.2f MB\n",
                mem.private_bytes / (1024.0 * 1024.0), mem.resident_bytes / (1024.0 * 1024.0));

    const double t0 = platform::monotonic_now_seconds();
    volatile long spin = 0;
    for (long i = 0; i < 10000000; ++i) spin += i;
    const double t1 = platform::monotonic_now_seconds();
    if (!(t1 > t0)) {
        std::fprintf(stderr, "FAIL: clock did not advance across a busy loop\n");
        return 1;
    }
    std::printf("clock: elapsed %.6f s over a busy loop (sanity check, should be small and nonzero)\n",
                t1 - t0);

    const std::string base = platform::get_base_path();
    if (base.empty()) {
        std::fprintf(stderr, "FAIL: get_base_path() returned empty\n");
        return 1;
    }
    std::printf("base path: %s\n", base.c_str());

    const std::string pref = platform::get_pref_path("baseball-sandbox", "platform-memory-test");
    if (pref.empty()) {
        std::fprintf(stderr, "FAIL: get_pref_path() returned empty\n");
        return 1;
    }
    std::printf("pref path: %s\n", pref.c_str());

    return 0;
}
