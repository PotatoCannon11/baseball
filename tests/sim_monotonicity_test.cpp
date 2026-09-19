#include <cstdio>

#include "bat_radius_at_y.h"
#include "sim/bat_geometry.h"
#include "sim/collision.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

double exit_speed_for_bat_speed(double bat_speed_mps) {
    sim::BatProperties bat_props;
    sim::BallProperties ball_props;

    sim::BatState bat{};
    bat.pivot_position = sim::Vec3{0.0, 0.0, 0.0};
    bat.orientation = sim::Quat::identity();
    const double contact_y = 0.75;
    bat.angular_velocity = sim::Vec3{bat_speed_mps / contact_y, 0.0, 0.0};

    const sim::BatCapsuleChain chain = sim::build_bat_capsule_chain(bat_props, bat);

    sim::BallState ball{};
    ball.position =
        sim::Vec3{0.0, contact_y, ball_props.radius_m + bat_radius_at_y(bat_props, contact_y) + 0.008};
    ball.velocity = sim::Vec3{0.0, 0.0, -40.23};  // fixed 90 mph pitch, fixed contact geometry

    const sim::ContactOutcome outcome = sim::resolve_bat_contact(ball, chain, bat, bat_props, ball_props, 0.02);
    return outcome.ball.velocity.length();
}

}  // namespace

// "Monotonicity: a harder swing never produces a shorter hit at fixed
// contact geometry." Fixed pitch and contact point, swing speed swept
// upward; exit speed must never decrease.
int main() {
    bool ok = true;

    const double bat_speeds[] = {5.0, 15.0, 25.0, 31.29, 40.0, 50.0, 60.0};
    double prev_exit_speed = -1.0;

    for (double bat_speed : bat_speeds) {
        const double exit_speed = exit_speed_for_bat_speed(bat_speed);
        std::printf("bat speed=%.2f m/s -> exit speed=%.4f m/s\n", bat_speed, exit_speed);
        if (prev_exit_speed >= 0.0) {
            ok &= check(exit_speed >= prev_exit_speed - 1e-9,
                        "exit speed must not decrease as swing speed increases");
        }
        prev_exit_speed = exit_speed;
    }

    if (ok) {
        std::printf("PASS: exit speed is monotonically non-decreasing in swing speed\n");
        return 0;
    }
    return 1;
}
