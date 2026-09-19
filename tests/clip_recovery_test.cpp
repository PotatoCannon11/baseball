#include "input/clip_recovery.h"

#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;
    input::ClipRecoveryWindow<9> window;

    // A smooth quadratic bump peaking at the window's center (index 4):
    // y = 100 - (x - 4)^2, so the true peak value is 100. Simulate the
    // sensor saturating at 80 for just the center sample (as if the real
    // peak exceeded the device's clip threshold), flagged via clip_flags
    // bit 0 (gyro x axis). Recovery should reconstruct something close to
    // the true 100, not the clipped 80.
    const float true_peak = 100.0f;
    bool got_output = false;
    input::ImuSample recovered{};
    for (int x = 0; x < 9; ++x) {
        input::ImuSample s{};
        const float dx = static_cast<float>(x - 4);
        const float true_value = true_peak - dx * dx;
        if (x == 4) {
            s.gyro[0] = 80.0f;  // clipped/saturated reading
            s.clip_flags = 0x01;
        } else {
            s.gyro[0] = true_value;
        }
        got_output = window.push(s, &recovered);
    }

    ok &= check(got_output, "window should produce output once filled (9 pushes into a size-9 window)");
    ok &= check(recovered.gyro[0] > 95.0f && recovered.gyro[0] < 105.0f,
                "recovered center value should be close to the true peak (100), not the clipped reading (80)");
    std::printf("recovered center value: %.3f (true peak 100, clipped reading was 80)\n", recovered.gyro[0]);

    // An unclipped sample should pass through completely unmodified.
    input::ClipRecoveryWindow<9> window2;
    input::ImuSample passthrough{};
    for (int x = 0; x < 9; ++x) {
        input::ImuSample s{};
        s.gyro[1] = static_cast<float>(x) * 2.0f;  // no clipping at all
        window2.push(s, &passthrough);
    }
    ok &= check(passthrough.gyro[1] == 8.0f, "unclipped center sample (index 4 -> value 8.0) should pass through unmodified");

    // Regression test: a short burst of fewer than WindowSize samples
    // (e.g. a quick mouse drag that ends before 9 motion events have
    // fired) must still produce output starting from the very FIRST push,
    // not stay silent until 9 cumulative pushes have happened. This
    // exactly caused the mouse backend to report 0.00 for an entire
    // click-drag-release gesture that was too short to fill the window.
    {
        input::ClipRecoveryWindow<9> burst_window;
        input::ImuSample out{};
        bool first_push_produced_output = false;
        for (int i = 0; i < 3; ++i) {
            input::ImuSample s{};
            s.gyro[0] = 42.0f + static_cast<float>(i);  // distinguishable nonzero values
            const bool produced = burst_window.push(s, &out);
            if (i == 0) first_push_produced_output = produced;
        }
        ok &= check(first_push_produced_output, "the very first push into a fresh window must produce output immediately");
        ok &= check(out.gyro[0] == 42.0f,
                    "with fewer than WindowSize samples ever seen, output should be the oldest buffered raw sample "
                    "(no correction attempted, but NOT withheld)");
        std::printf("burst-of-3 test: first push output=%s, third push value=%.1f (expected 42.0, the oldest sample)\n",
                    first_push_produced_output ? "yes" : "no", out.gyro[0]);
    }

    if (ok) {
        std::printf("PASS: clip recovery reconstructs a clipped peak and leaves unclipped samples alone\n");
        return 0;
    }
    return 1;
}
