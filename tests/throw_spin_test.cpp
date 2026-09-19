#include <cmath>
#include <cstdio>

#include "sim/throw.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;

    // --- Grip index selection from buttons ---
    using common::InputButton;
    ok &= check(sim::grip_index_from_buttons(0) == 0, "no grip button held should select the default (index 0)");
    ok &= check(sim::grip_index_from_buttons(static_cast<std::uint16_t>(InputButton::kGripPreset2)) == 1,
                "kGripPreset2 should select index 1");
    ok &= check(sim::grip_index_from_buttons(static_cast<std::uint16_t>(InputButton::kGripPreset4)) == 3,
                "kGripPreset4 should select index 3");

    // --- Grip matrix mapping ---
    sim::GripPreset grip;
    grip.matrix[0] = 2.0;
    grip.matrix[4] = 0.0;  // suppress y
    grip.matrix[8] = 1.0;
    grip.efficiency = 1.0;
    const sim::Vec3 mapped = grip.apply(sim::Vec3{3.0, 5.0, 7.0});
    ok &= check(std::fabs(mapped.x - 6.0) < 1e-9 && std::fabs(mapped.y - 0.0) < 1e-9 &&
                    std::fabs(mapped.z - 7.0) < 1e-9,
                "grip matrix should linearly map controller angular velocity to spin");

    // --- Finger offset changes spin (stick_x/stick_y != 0 vs 0) ---
    sim::ArmState arm{};
    arm.pivot_position = sim::Vec3{0.0, 1.8, 18.0};
    arm.orientation = sim::Quat::identity();
    arm.angular_velocity = sim::Vec3{15.0, 0.0, 0.0};  // some hand motion so v_measured > 0

    sim::ArmProperties arm_props;
    sim::BallProperties ball_props;
    sim::GripPreset neutral_grip;  // identity matrix, zero speed penalty
    neutral_grip.matrix[0] = neutral_grip.matrix[4] = neutral_grip.matrix[8] = 0.0;  // isolate finger-impulse spin

    common::PlayerInput input_centered{};
    common::PlayerInput input_offset{};
    input_offset.stick_x = static_cast<std::int16_t>(1.0f * common::PlayerInput::kStickScale);

    const sim::ReleaseState release_centered = sim::release_state_from_arm(arm, input_centered, arm_props, 10);
    const sim::ReleaseState release_offset = sim::release_state_from_arm(arm, input_offset, arm_props, 10);

    const sim::BallState ball_centered = sim::throw_ball(release_centered, arm_props, ball_props, neutral_grip);
    const sim::BallState ball_offset = sim::throw_ball(release_offset, arm_props, ball_props, neutral_grip);

    std::printf("centered spin=(%.4f,%.4f,%.4f) offset spin=(%.4f,%.4f,%.4f)\n", ball_centered.angular_velocity.x,
                ball_centered.angular_velocity.y, ball_centered.angular_velocity.z, ball_offset.angular_velocity.x,
                ball_offset.angular_velocity.y, ball_offset.angular_velocity.z);
    ok &= check((ball_offset.angular_velocity - ball_centered.angular_velocity).length() > 1e-6,
                "offsetting the stick should change the resulting spin (finger-impulse contribution)");

    // --- Finger impulse is capped ---
    sim::ArmProperties uncapped_props = arm_props;
    uncapped_props.finger_impulse_per_mps = 1000.0;  // would be huge without the cap
    const sim::BallState ball_capped = sim::throw_ball(release_offset, uncapped_props, ball_props, neutral_grip);
    sim::ArmProperties tighter_cap = uncapped_props;
    tighter_cap.finger_impulse_max = uncapped_props.finger_impulse_max * 0.5;
    const sim::BallState ball_tighter = sim::throw_ball(release_offset, tighter_cap, ball_props, neutral_grip);
    ok &= check(ball_tighter.angular_velocity.length() < ball_capped.angular_velocity.length(),
                "a lower finger-impulse cap should produce less spin");

    if (ok) {
        std::printf("PASS: grip selection, matrix mapping, and finger-impulse spin all behave as specified\n");
        return 0;
    }
    return 1;
}
