#include "sim/throw.h"

#include <algorithm>

#include "sim/math.h"

namespace sim {

int grip_index_from_buttons(std::uint16_t buttons) {
    using common::InputButton;
    if (buttons & static_cast<std::uint16_t>(InputButton::kGripPreset4)) return 3;
    if (buttons & static_cast<std::uint16_t>(InputButton::kGripPreset3)) return 2;
    if (buttons & static_cast<std::uint16_t>(InputButton::kGripPreset2)) return 1;
    return 0;  // kGripPreset1, or no preset held -- four-seam default
}

ReleaseState release_state_from_arm(const ArmState& arm, const common::PlayerInput& input,
                                     const ArmProperties& arm_props, std::uint32_t release_tick) {
    ReleaseState r{};

    const Vec3 lever = arm.orientation.rotate(Vec3{0.0, arm_props.virtual_arm_length_m, 0.0});
    r.release_point = arm.pivot_position + lever;
    // "Release speed = arm angular velocity x virtual arm length" -- no
    // double integration of acceleration, same reasoning as the bat model.
    r.hand_velocity = arm.angular_velocity.cross(lever);

    r.controller_angular_velocity = arm.angular_velocity;

    // "Wrist motion at release (forearm-axis component, wrist snap)
    // blends into the spin axis": the component of the hand's angular
    // velocity ABOUT the arm's own axis (twisting the forearm) is exactly
    // a wrist-snap motion, as opposed to the swing-plane rotation that
    // mostly determines hand_velocity's direction.
    const Vec3 forearm_axis_world = arm.orientation.rotate(Vec3{0.0, 1.0, 0.0});
    const double snap_component = arm.angular_velocity.dot(forearm_axis_world);
    r.wrist_snap_axis = forearm_axis_world * snap_component;

    r.stick_x = static_cast<double>(input.stick_x) / static_cast<double>(common::PlayerInput::kStickScale);
    r.stick_y = static_cast<double>(input.stick_y) / static_cast<double>(common::PlayerInput::kStickScale);
    r.grip_preset_index = grip_index_from_buttons(input.buttons);
    r.release_tick = release_tick;

    return r;
}

BallState throw_ball(const ReleaseState& release, const ArmProperties& arm_props, const BallProperties& ball_props,
                      const GripPreset& grip) {
    BallState out{};
    out.position = release.release_point;
    out.orientation = Quat::identity();

    const double v_measured = release.hand_velocity.length();

    // "Heavier or bigger balls need more effort": the same arm motion
    // (v_measured) yields less actual ball speed as the ball's mass grows
    // past the reference the player's real-world throwing motion is
    // calibrated against.
    const double mass_scale =
        math::sqrt((arm_props.M_arm_kg + arm_props.reference_ball_mass_kg) / (arm_props.M_arm_kg + ball_props.mass_kg));
    const double release_speed = v_measured * mass_scale * (1.0 - grip.speed_penalty);

    const Vec3 direction = v_measured > 1e-9 ? release.hand_velocity * (1.0 / v_measured) : Vec3{0.0, 0.0, -1.0};
    out.velocity = direction * release_speed;

    // Spin: grip matrix maps the raw controller angular velocity to a
    // spin contribution (scaled by the grip's energy-transfer
    // efficiency), blended additively with the wrist-snap axis
    // contribution and the finger-impulse contribution below. Additive
    // blending (rather than, say, a weighted average) is a deliberate
    // simplification -- it means a grip and a wrist snap can reinforce or
    // partially cancel, same as how a real pitcher's grip and wrist
    // action compound.
    const Vec3 grip_spin = grip.apply(release.controller_angular_velocity) * grip.efficiency;

    // "The stick offsets finger contact point": r, the offset from the
    // ball's center where the fingers are still in contact at release.
    const Vec3 finger_offset = (Vec3{release.stick_x, release.stick_y, 0.0}) *
                                (arm_props.finger_offset_scale * ball_props.radius_m);

    // "Spin = finger impulse x r / I ... capped by maximum finger
    // impulse." The impulse's magnitude scales with release speed (a
    // faster snap plausibly drives more finger force into the ball) up to
    // the configured cap; its direction is taken along the grip's default
    // spin axis (the direction that grip's finger action characteristically
    // snaps in) -- a modeling choice, not something the spec pins down
    // itself.
    const double impulse_mag =
        std::min(arm_props.finger_impulse_max, arm_props.finger_impulse_per_mps * v_measured);
    const Vec3 finger_impulse = grip.default_axis.normalized() * impulse_mag;
    const double moment_of_inertia = ball_props.moment_of_inertia();
    const Vec3 finger_spin =
        moment_of_inertia > 1e-12 ? finger_offset.cross(finger_impulse) * (1.0 / moment_of_inertia) : Vec3{};

    out.angular_velocity = grip_spin + release.wrist_snap_axis + finger_spin;

    return out;
}

}  // namespace sim
