#include "input/bat_model.h"

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

    // |omega| * r for a single-axis rotation: omega=(0,0,20 rad/s), r=0.75m
    // -> 15 m/s.
    {
        const float omega[3] = {0.0f, 0.0f, 20.0f};
        const float speed = input::barrel_speed_mps(omega, 0.75f);
        ok &= check(std::fabs(speed - 15.0f) < 1e-4f, "single-axis rotation should give |omega|*r exactly");
    }

    // Multi-axis: magnitude should be the vector norm, not the sum.
    {
        const float omega[3] = {3.0f, 4.0f, 0.0f};  // |omega| = 5
        const float speed = input::barrel_speed_mps(omega, 2.0f);
        ok &= check(std::fabs(speed - 10.0f) < 1e-4f, "multi-axis rotation should use vector magnitude (3-4-5 triangle)");
    }

    // A "hard swing" scaled input should land in the spec's expected
    // 25-40 m/s range at the default arm length -- sanity check that the
    // constants are at least in the right ballpark, not a hardware
    // measurement (that needs a real Joy-Con).
    {
        const float hard_swing_omega[3] = {0.0f, 0.0f, 40.0f};  // ~40 rad/s, a fast swing
        const float speed = input::barrel_speed_mps(hard_swing_omega, input::kDefaultArmLengthMeters);
        ok &= check(speed > 20.0f && speed < 50.0f,
                    "a plausible fast angular velocity at the default arm length should land near the spec's 25-40 m/s hard-swing range");
        std::printf("plausibility check: omega=40 rad/s, arm=%.2f m -> %.2f m/s\n", input::kDefaultArmLengthMeters,
                    speed);
    }

    if (ok) {
        std::printf("PASS: bat model computes |omega x r| correctly\n");
        return 0;
    }
    return 1;
}
