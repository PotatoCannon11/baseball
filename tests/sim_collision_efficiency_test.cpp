#include <cmath>
#include <cstdio>

#include "bat_radius_at_y.h"
#include "sim/bat_geometry.h"
#include "sim/collision.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// "Collision efficiency: exit speed = q*v_pitch + (1+q)*v_bat,
// q = (e-r)/(1+r), r = m_ball/M_eff. For q about 0.2, a 90 mph pitch and
// 70 mph bat gives roughly 100 mph." A purely 1D head-on setup (bat
// swinging directly toward an incoming pitch) so the idealized formula
// applies exactly to what resolve_bat_contact computes.
int main() {
    bool ok = true;

    sim::BatProperties bat_props;
    sim::BallProperties ball_props;

    sim::BatState bat{};
    bat.pivot_position = sim::Vec3{0.0, 0.0, 0.0};
    bat.orientation = sim::Quat::identity();
    const double bat_speed_mps = 31.29;  // 70 mph
    const double r_contact = 0.75;       // barrel sweet-spot region of the default profile
    bat.angular_velocity = sim::Vec3{bat_speed_mps / r_contact, 0.0, 0.0};

    const sim::BatCapsuleChain chain = sim::build_bat_capsule_chain(bat_props, bat);

    sim::BallState ball{};
    const double pitch_speed_mps = 40.23;  // 90 mph
    ball.position = sim::Vec3{0.0, 0.75, ball_props.radius_m + bat_radius_at_y(bat_props, 0.75) + 0.008};
    ball.velocity = sim::Vec3{0.0, 0.0, -pitch_speed_mps};

    const sim::ContactOutcome outcome =
        sim::resolve_bat_contact(ball, chain, bat, bat_props, ball_props, 0.02);

    ok &= check(outcome.contact_occurred, "contact should occur for a ball aimed at the bat");

    // M_eff from the same formula the collision code uses: the ball's own
    // r x n term is zero (n is radial from a sphere), leaving only the
    // bat's rotational compliance about its pivot.
    const sim::Vec3 r_bat{0.0, r_contact, 0.0};
    const sim::Vec3 n{0.0, 0.0, 1.0};
    const double r_cross_n = r_bat.cross(n).length();
    const double i_pivot = bat_props.moment_of_inertia_about_pivot();
    const double m_eff = i_pivot / (r_cross_n * r_cross_n);
    const double r = ball_props.mass_kg / m_eff;

    // Effective-momentum conservation: m*v1 + M_eff*v2 is conserved by
    // construction of the effective-mass contact mechanics (verified
    // independently of whatever the Hunt-Crossley damping happens to
    // produce for "e"). This is the actual physical claim milestone 4's
    // effective-mass formula makes; check it holds exactly.
    const double v1_before = ball.velocity.dot(n);
    const double v2_before = bat.angular_velocity.cross(r_bat).dot(n);
    const double v1_after = outcome.ball.velocity.dot(n);
    const sim::Vec3 bat_omega_after = bat.angular_velocity + outcome.bat_angular_velocity_delta;
    const double v2_after = bat_omega_after.cross(r_bat).dot(n);

    const double momentum_before = ball_props.mass_kg * v1_before + m_eff * v2_before;
    const double momentum_after = ball_props.mass_kg * v1_after + m_eff * v2_after;
    std::printf("effective momentum before=%.6f after=%.6f\n", momentum_before, momentum_after);
    ok &= check(std::fabs(momentum_after - momentum_before) < 1e-6 * std::fabs(momentum_before),
                "effective momentum (m*v1 + M_eff*v2) must be conserved through contact");

    // Exit speed vs. the spec's own worked example: q ~= 0.2 for these
    // inputs should give "roughly 100 mph". Generous tolerance (80-120
    // mph) since the actual q here is whatever this bat's real geometry
    // implies, not exactly 0.2, and "roughly" is the spec's own word.
    const double exit_speed_mps = outcome.ball.velocity.length();
    const double exit_speed_mph = exit_speed_mps * 2.23694;
    std::printf("r=%.4f M_eff=%.4f kg exit speed=%.2f mph (spec: ~100 mph)\n", r, m_eff, exit_speed_mph);
    ok &= check(exit_speed_mph > 80.0 && exit_speed_mph < 120.0,
                "a 90 mph pitch + 70 mph swing should give roughly 100 mph exit speed");

    ok &= check(outcome.system_energy_after_j <= outcome.system_energy_before_j + 1e-6,
                "contact must not manufacture energy");

    if (ok) {
        std::printf("PASS: collision efficiency matches the effective-mass formula\n");
        return 0;
    }
    return 1;
}
