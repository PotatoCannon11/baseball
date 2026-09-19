#include "input/motion_pipeline.h"

#include <cmath>
#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

// Pushes `count` synthetic samples at `rate_hz`, starting at time_ns,
// advancing *time_ns as it goes.
void push_synthetic_samples(input::ImuRing& ring, int count, float gyro_x, float gyro_y, float gyro_z,
                             double rate_hz, std::uint64_t* time_ns) {
    const std::uint64_t dt_ns = static_cast<std::uint64_t>(1e9 / rate_hz);
    for (int i = 0; i < count; ++i) {
        input::ImuSample s{};
        s.timestamp_ns = *time_ns;
        s.gyro[0] = gyro_x;
        s.gyro[1] = gyro_y;
        s.gyro[2] = gyro_z;
        ring.push(s);
        *time_ns += dt_ns;
    }
}

}  // namespace

// End-to-end test of the exact class tools/barrel_speed uses: synthetic
// "at rest" samples for calibration, then a synthetic constant-angular-
// velocity "swing", checking the resulting barrel speed matches the known
// input within the bat model's simple |omega|*r formula. This is the
// numeric core of the milestone-2 acceptance test, run without any
// hardware or SDL dependency at all.
int main() {
    bool ok = true;

    input::MotionPipeline pipeline;
    pipeline.set_arm_length(0.75f);
    pipeline.start_bias_calibration();

    input::ImuRing ring;
    std::uint64_t time_ns = 1'000'000'000ull;
    constexpr double kRateHz = 240.0;

    // Simulate a small constant true bias plus "at rest" (zero true
    // rotation) for calibration, matching how bias_calibration_test.cpp
    // validates GyroBiasCalibrator in isolation.
    const float true_bias[3] = {0.01f, -0.02f, 0.005f};
    push_synthetic_samples(ring, 300, true_bias[0], true_bias[1], true_bias[2], kRateHz, &time_ns);
    pipeline.process(ring);
    pipeline.finish_bias_calibration();

    ok &= check(!pipeline.bias_calibrating(), "pipeline should report calibration finished after finish_bias_calibration()");

    // Now simulate a "swing": a constant 25 rad/s about a single axis for
    // half a second (rate high enough that the Madgwick filter's own
    // integration lag is negligible by the last sample).
    const float swing_omega = 25.0f;
    push_synthetic_samples(ring, 120, 0.0f, 0.0f, swing_omega + true_bias[2], kRateHz, &time_ns);
    pipeline.process(ring);

    const float expected_speed = swing_omega * 0.75f;  // |omega| * r
    const float actual_speed = pipeline.barrel_speed_mps();
    std::printf("synthetic swing: omega=%.1f rad/s, arm=0.75 m -> expected %.2f m/s, pipeline reported %.2f m/s\n",
                swing_omega, expected_speed, actual_speed);
    ok &= check(std::fabs(actual_speed - expected_speed) < 0.5f,
                "pipeline's reported barrel speed should match |omega|*r for a known constant swing, after bias correction");

    // The gyro bias should have been genuinely subtracted, not just
    // coincidentally cancelled: last_gyro()[2] should be close to the true
    // swing rate, not the biased raw rate.
    ok &= check(std::fabs(pipeline.last_gyro()[2] - swing_omega) < 0.5f,
                "bias-corrected gyro z should match the true swing rate, not the raw biased reading");

    // Regression test for the reported "mouse backend does nothing" bug:
    // a source that's idle until motion starts (like the mouse-drag dev
    // backend) can deliver ZERO samples during the calibration window if
    // the user hasn't started dragging yet. Calibration must still
    // complete and let real samples through afterward, not swallow every
    // subsequent sample into calibration forever.
    {
        input::MotionPipeline idle_calibration_pipeline;
        idle_calibration_pipeline.set_arm_length(0.75f);
        idle_calibration_pipeline.start_bias_calibration();

        input::ImuRing empty_ring;  // nothing pushed: simulates an idle mouse, no drag yet
        idle_calibration_pipeline.process(empty_ring);
        idle_calibration_pipeline.finish_bias_calibration();
        ok &= check(!idle_calibration_pipeline.bias_calibrating(),
                    "calibration must complete even with zero samples observed (e.g. mouse never dragged during the window)");

        input::ImuRing swing_ring;
        std::uint64_t t = 5'000'000'000ull;
        const float omega = 30.0f;
        push_synthetic_samples(swing_ring, 60, 0.0f, 0.0f, omega, kRateHz, &t);
        idle_calibration_pipeline.process(swing_ring);

        const float speed = idle_calibration_pipeline.barrel_speed_mps();
        ok &= check(speed > 1.0f,
                    "a swing arriving after zero-sample calibration must still reach the bat model and report nonzero speed "
                    "(this is exactly the bug: it used to stay stuck at 0.00 forever)");
        std::printf("idle-calibration regression: swing after zero-sample calibration -> %.2f m/s (must be > 0)\n", speed);
    }

    // Regression test for the actual reported bug: a short mouse
    // click-drag-release gesture that produces only a HANDFUL of motion
    // samples (fewer than the clip recovery window's size) should still
    // update barrel speed starting from its first real sample -- not
    // require ever having accumulated WindowSize samples across the
    // pipeline's whole lifetime, which a brief drag might never reach.
    {
        input::MotionPipeline short_burst_pipeline;
        short_burst_pipeline.set_arm_length(0.75f);
        short_burst_pipeline.start_bias_calibration();

        input::ImuRing empty_ring;
        short_burst_pipeline.process(empty_ring);
        short_burst_pipeline.finish_bias_calibration();

        // A quick drag: only 3 motion samples, well under the 9-sample
        // clip recovery window.
        input::ImuRing short_ring;
        std::uint64_t t = 9'000'000'000ull;
        push_synthetic_samples(short_ring, 3, 0.0f, 0.0f, 15.0f, kRateHz, &t);
        short_burst_pipeline.process(short_ring);

        const float speed = short_burst_pipeline.barrel_speed_mps();
        ok &= check(speed > 1.0f,
                    "a short (3-sample) drag must update barrel speed immediately, not stay at 0.00 because the "
                    "clip recovery window never filled");
        std::printf("short-burst regression: 3-sample drag -> %.2f m/s (must be > 0)\n", speed);
    }

    if (ok) {
        std::printf("PASS: MotionPipeline (calibration -> clip recovery -> filter -> bat model) reproduces a known synthetic swing's speed\n");
        return 0;
    }
    return 1;
}
