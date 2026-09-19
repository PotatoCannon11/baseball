#include "alloc/alloc_counter.h"

#include <cstdio>
#include <vector>

// Milestone 1 check: the global operator new/delete override actually
// tracks heap activity. The real "zero allocations during 60 s of sim"
// test comes later once the sim exists (milestone 3+); this just proves
// the counter itself works.
int main() {
    const alloc::AllocStats before = alloc::get_stats();

    std::vector<int> v;
    v.reserve(1000);
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    volatile int sink = v[999];
    (void)sink;

    const alloc::AllocStats mid = alloc::get_stats();
    if (mid.total_allocations <= before.total_allocations) {
        std::fprintf(stderr, "FAIL: expected allocation count to increase after heap use\n");
        return 1;
    }
    if (mid.bytes_allocated <= before.bytes_allocated) {
        std::fprintf(stderr, "FAIL: expected bytes_allocated to increase\n");
        return 1;
    }

    v.clear();
    v.shrink_to_fit();  // should free the reserved buffer

    const alloc::AllocStats after = alloc::get_stats();
    if (after.total_frees <= mid.total_frees) {
        std::fprintf(stderr, "FAIL: expected free count to increase after shrink_to_fit\n");
        return 1;
    }

    std::printf(
        "PASS: alloc_counter tracked %llu allocations / %llu frees, "
        "%llu bytes allocated / %llu bytes freed over the test\n",
        static_cast<unsigned long long>(after.total_allocations - before.total_allocations),
        static_cast<unsigned long long>(after.total_frees - before.total_frees),
        static_cast<unsigned long long>(after.bytes_allocated - before.bytes_allocated),
        static_cast<unsigned long long>(after.bytes_freed - before.bytes_freed));
    return 0;
}
