#include <cmath>
#include <cstdio>

#include "sim/flight.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

// Range of a well-hit fly ball, launched at launch_angle_rad and
// speed_mps, integrated until it returns to y=0. drag_on/magnus_on toggle
// the aerodynamic forces (via roughness=0 disabling the drag-crisis
// transition isn't enough by itself -- zeroing both Cd terms and spin is
// what actually gives a vacuum trajectory).
double simulate_range(double speed_mps, double launch_angle_rad, bool with_air) {
    sim::BallProperties ball;
    sim::Environment env;
    if (!with_air) {
        ball.drag_coefficient_subcritical = 0.0;
        ball.drag_coefficient_supercritical = 0.0;
        ball.magnus_lift_slope = 0.0;
        env.temperature_celsius = 20.0;  // air_density_kgpm3() still > 0, but zero Cd/Cl means it does nothing
    }

    sim::BallState ball_state{};
    ball_state.position = sim::Vec3{0.0, 0.0001, 0.0};  // just above ground so the first tick isn't a "landing"
    ball_state.velocity = sim::Vec3{speed_mps * std::cos(launch_angle_rad), speed_mps * std::sin(launch_angle_rad), 0.0};
    ball_state.orientation = sim::Quat::identity();
    // A small amount of backspin, typical of a well-struck fly ball --
    // needed for the Magnus force to matter at all, and realistic (a pure
    // topspin/backspin-free "carry" test would understate real carry
    // loss/gain from lift).
    if (with_air) ball_state.angular_velocity = sim::Vec3{0.0, 0.0, 200.0};

    const double gravity = 9.80665;
    const double dt = 1.0 / 240.0;
    for (int i = 0; i < 100000; ++i) {
        const sim::BallState next = sim::integrate_flight_rk4(ball_state, ball, env, gravity, dt);
        if (next.position.y <= 0.0 && ball_state.position.y > 0.0) {
            // Linear interpolation to the exact landing point, same
            // reasoning as apply_ground_contact's crossing estimate.
            const double t_frac = ball_state.position.y / (ball_state.position.y - next.position.y);
            return ball_state.position.x + (next.position.x - ball_state.position.x) * t_frac;
        }
        ball_state = next;
    }
    return ball_state.position.x;  // didn't land in range (shouldn't happen); best-effort
}

}  // namespace

// "Carry: a well-hit fly ball travels roughly a third to a half less than
// vacuum range." i.e. actual range should be between 50% and 67% of the
// no-air (vacuum) range for typical "well-hit" launch conditions.
int main() {
    bool ok = true;

    const double speed_mps = 44.7;          // ~100 mph exit velocity
    const double launch_angle = 30.0 * (sim::math::kPi / 180.0);  // classic "ideal" home run launch angle

    const double vacuum_range = simulate_range(speed_mps, launch_angle, false);
    const double real_range = simulate_range(speed_mps, launch_angle, true);

    const double ratio = real_range / vacuum_range;
    std::printf("vacuum range=%.2f m, real range=%.2f m, ratio=%.4f (spec: ~0.50-0.67)\n", vacuum_range, real_range,
                ratio);

    // Sanity: vacuum range should match the textbook v^2*sin(2*theta)/g
    // formula (confirms integrate_flight_rk4 itself is correct before
    // trusting the drag comparison).
    const double expected_vacuum = (speed_mps * speed_mps * std::sin(2.0 * launch_angle)) / 9.80665;
    std::printf("expected vacuum range (v^2 sin(2theta)/g)=%.2f m\n", expected_vacuum);
    ok &= check(std::fabs(vacuum_range - expected_vacuum) < 0.5,
                "vacuum-range integration should match the textbook projectile-range formula");

    ok &= check(ratio > 0.40 && ratio < 0.75,
                "a well-hit fly ball should carry roughly a third to a half less than its vacuum range");

    if (ok) {
        std::printf("PASS: carry distance is in the expected range relative to vacuum\n");
        return 0;
    }
    return 1;
}
