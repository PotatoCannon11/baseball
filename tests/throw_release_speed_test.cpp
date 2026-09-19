#include <cmath>
#include <cstdio>

#include "sim/throw.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

sim::ReleaseState make_release(double omega_mag) {
    sim::ArmState arm{};
    arm.pivot_position = sim::Vec3{0.0, 1.8, 18.0};
    arm.orientation = sim::Quat::identity();
    arm.angular_velocity = sim::Vec3{omega_mag, 0.0, 0.0};

    common::PlayerInput input{};
    return sim::release_state_from_arm(arm, input, sim::ArmProperties{}, 100);
}

}  // namespace

// "Release speed = arm angular velocity x virtual arm length" and
// "Heavier or bigger balls need more effort: v = v_measured *
// sqrt((M_arm + m_ref) / (M_arm + m))."
int main() {
    bool ok = true;

    sim::ArmProperties arm_props;
    sim::GripPreset grip;  // identity/no penalty default

    // omega about world x, lever along local +y (world +y since
    // orientation is identity) -- hand_velocity = omega x lever, purely
    // in z, magnitude = omega * lever_length.
    const double omega = 30.0;
    const sim::ReleaseState release = make_release(omega);
    const double expected_v_measured = omega * arm_props.virtual_arm_length_m;
    ok &= check(std::fabs(release.hand_velocity.length() - expected_v_measured) < 1e-9,
                "hand velocity should equal omega x virtual arm length");

    // Reference-mass ball: no speed adjustment.
    sim::BallProperties ref_ball;
    ref_ball.mass_kg = arm_props.reference_ball_mass_kg;
    const sim::BallState ref_result = sim::throw_ball(release, arm_props, ref_ball, grip);
    std::printf("v_measured=%.4f reference-mass release speed=%.4f\n", expected_v_measured,
                ref_result.velocity.length());
    ok &= check(std::fabs(ref_result.velocity.length() - expected_v_measured) < 1e-6,
                "a reference-mass ball should get exactly v_measured (no scaling)");

    // Heavier ball: strictly less speed for the identical arm motion.
    sim::BallProperties heavy_ball;
    heavy_ball.mass_kg = ref_ball.mass_kg * 3.0;
    const sim::BallState heavy_result = sim::throw_ball(release, arm_props, heavy_ball, grip);
    std::printf("heavy-ball (%.3fx mass) release speed=%.4f\n", heavy_ball.mass_kg / ref_ball.mass_kg,
                heavy_result.velocity.length());
    ok &= check(heavy_result.velocity.length() < ref_result.velocity.length(),
                "a heavier ball should come out slower for the same arm motion");

    // Lighter ball: strictly more speed.
    sim::BallProperties light_ball;
    light_ball.mass_kg = ref_ball.mass_kg * 0.3;
    const sim::BallState light_result = sim::throw_ball(release, arm_props, light_ball, grip);
    std::printf("light-ball (%.3fx mass) release speed=%.4f\n", light_ball.mass_kg / ref_ball.mass_kg,
                light_result.velocity.length());
    ok &= check(light_result.velocity.length() > ref_result.velocity.length(),
                "a lighter ball should come out faster for the same arm motion");

    // Exact formula check against the reference ball's result.
    const double expected_heavy_v = expected_v_measured *
        std::sqrt((arm_props.M_arm_kg + arm_props.reference_ball_mass_kg) / (arm_props.M_arm_kg + heavy_ball.mass_kg));
    ok &= check(std::fabs(heavy_result.velocity.length() - expected_heavy_v) < 1e-6,
                "heavy-ball speed should match the exact mass/size scaling formula");

    if (ok) {
        std::printf("PASS: release speed follows the arm-kinematics and mass-scaling formulas\n");
        return 0;
    }
    return 1;
}
