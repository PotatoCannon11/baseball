#include "input/madgwick_filter.h"

#include <cmath>
#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;

    // Test 1: pure gyro integration (no accelerometer correction) about a
    // single axis should match the analytic quaternion for that rotation
    // angle. This is the check most likely to catch a transcription error
    // in the gradient-descent formulas (a sign flip would show up as the
    // wrong rotation direction or the wrong axis moving).
    {
        input::MadgwickFilter filter;
        const float omega = 1.0f;  // rad/s about z
        const float dt = 1.0f / 1000.0f;
        const int steps = 1000;  // 1 second total -> expect a 1 radian rotation
        const float gyro[3] = {0.0f, 0.0f, omega};
        const float no_accel[3] = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < steps; ++i) filter.update(gyro, no_accel, dt);

        float q[4];
        filter.quaternion(q);  // (w, x, y, z)
        const float expected_w = std::cos(0.5f);  // cos(theta/2), theta = omega*T = 1 rad
        const float expected_z = std::sin(0.5f);  // sin(theta/2)

        ok &= check(std::fabs(q[0] - expected_w) < 0.01f, "pure z-gyro integration: w component should match cos(theta/2)");
        ok &= check(std::fabs(q[3] - expected_z) < 0.01f, "pure z-gyro integration: z component should match sin(theta/2)");
        ok &= check(std::fabs(q[1]) < 0.01f && std::fabs(q[2]) < 0.01f,
                    "pure z-gyro integration: x and y components should stay ~0 (rotation is only about z)");
        std::printf("pure gyro test: q=(%.4f, %.4f, %.4f, %.4f), expected w=%.4f z=%.4f\n", q[0], q[1], q[2],
                    q[3], expected_w, expected_z);
    }

    // Test 2: accelerometer correction should pull a disturbed orientation
    // back toward "level" when fed a steady (0,0,g) reading, whereas an
    // identical disturbance with NO accelerometer correction should not
    // move at all afterward (zero gyro input, zero correction -> no change).
    // This demonstrates the correction step actually does something,
    // without needing to hand-derive an exact target quaternion.
    {
        constexpr float kG = 9.80665f;
        const float dt = 1.0f / 240.0f;
        const float disturbance_gyro[3] = {1.0f, 0.0f, 0.0f};
        const float zero_gyro[3] = {0.0f, 0.0f, 0.0f};
        const float no_accel[3] = {0.0f, 0.0f, 0.0f};
        const float level_accel[3] = {0.0f, 0.0f, kG};

        input::MadgwickFilter no_correction;
        input::MadgwickFilter with_correction;

        // Apply the same disturbance to both, with no accel correction
        // available yet (simulates a brief period where accel can't be
        // trusted, e.g. mid-swing). ~100 steps at 1 rad/s and dt=1/240s is
        // about a 0.42 rad (~24 degree) tilt -- clearly away from identity.
        for (int i = 0; i < 100; ++i) {
            no_correction.update(disturbance_gyro, no_accel, dt);
            with_correction.update(disturbance_gyro, no_accel, dt);
        }

        float q_after_disturbance[4];
        with_correction.quaternion(q_after_disturbance);
        ok &= check(q_after_disturbance[0] < 0.99f, "disturbance should have visibly tilted the orientation away from identity");

        // Now: no_correction keeps getting zero gyro and zero accel (should
        // freeze exactly where it is). with_correction gets zero gyro but a
        // steady level accel reading (should be pulled back toward w=1).
        for (int i = 0; i < 2000; ++i) {
            no_correction.update(zero_gyro, no_accel, dt);
            with_correction.update(zero_gyro, level_accel, dt);
        }

        float q_no_correction[4];
        float q_with_correction[4];
        no_correction.quaternion(q_no_correction);
        with_correction.quaternion(q_with_correction);

        ok &= check(std::fabs(q_no_correction[0] - q_after_disturbance[0]) < 1e-5f,
                    "with zero gyro and zero accel, orientation should not change at all");
        ok &= check(q_with_correction[0] > q_no_correction[0],
                    "accelerometer correction toward a level reading should pull w back closer to 1 than no correction at all");
        std::printf("accel correction test: w after disturbance=%.4f, w with no further correction=%.4f, w pulled back=%.4f\n",
                    q_after_disturbance[0], q_no_correction[0], q_with_correction[0]);
    }

    if (ok) {
        std::printf("PASS: Madgwick filter gyro integration and accelerometer correction behave as expected\n");
        return 0;
    }
    return 1;
}
