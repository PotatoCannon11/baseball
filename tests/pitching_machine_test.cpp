#include <cmath>
#include <cstdio>

#include "sim/pitching_machine.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;

    const sim::Vec3 release{0.0, 1.8, 18.44};  // regulation pitching distance, meters
    const sim::Vec3 target{0.0, 0.9, 0.0};      // roughly the middle of the strike zone
    const double speed_mps = 40.23;             // 90 mph
    const sim::Vec3 spin{0.0, 0.0, 220.0};       // backspin, rad/s

    const sim::BallState ball = sim::pitching_machine_launch(release, target, speed_mps, spin);

    ok &= check(ball.position.x == release.x && ball.position.y == release.y && ball.position.z == release.z,
                "ball should launch from the release point");
    ok &= check(std::fabs(ball.velocity.length() - speed_mps) < 1e-9, "launch speed should match the requested speed");
    ok &= check(ball.angular_velocity.x == spin.x && ball.angular_velocity.y == spin.y &&
                    ball.angular_velocity.z == spin.z,
                "spin should pass through unchanged");

    const sim::Vec3 to_target = (target - release).normalized();
    const sim::Vec3 v_dir = ball.velocity.normalized();
    const double dot = to_target.dot(v_dir);
    ok &= check(dot > 0.999999, "velocity should point from the release point toward the target");

    if (ok) {
        std::printf("PASS: pitching machine launches the ball at the requested speed toward the target\n");
        return 0;
    }
    return 1;
}
