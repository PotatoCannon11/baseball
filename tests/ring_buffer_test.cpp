#include "common/ring_buffer.h"

#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;

    common::RingBuffer<int, 4> rb;
    ok &= check(rb.empty(), "new ring buffer should be empty");
    ok &= check(rb.size() == 0, "new ring buffer size should be 0");

    rb.push(1);
    rb.push(2);
    rb.push(3);
    ok &= check(rb.size() == 3, "size should be 3 after 3 pushes");
    ok &= check(rb.front() == 1, "front should be oldest (1)");
    ok &= check(rb.back() == 3, "back should be newest (3)");
    ok &= check(rb[0] == 1 && rb[1] == 2 && rb[2] == 3, "indexed access should be oldest-to-newest");

    // Overwrite past capacity: pushing 2 more (total 5) into capacity-4
    // should drop the oldest (1), leaving 2,3,4,5.
    rb.push(4);
    rb.push(5);
    ok &= check(rb.size() == 4, "size should saturate at capacity (4)");
    ok &= check(rb.front() == 2, "front should now be 2 (1 was overwritten)");
    ok &= check(rb.back() == 5, "back should be newest (5)");
    ok &= check(rb[0] == 2 && rb[1] == 3 && rb[2] == 4 && rb[3] == 5,
                "indexed access should reflect overwrite");
    ok &= check(rb.total_pushed() == 5, "total_pushed should count every push, including overwritten ones");

    rb.clear();
    ok &= check(rb.empty() && rb.size() == 0, "clear() should empty the buffer");

    if (ok) {
        std::printf("PASS: ring buffer push/overwrite/index/clear behave as expected\n");
        return 0;
    }
    return 1;
}
