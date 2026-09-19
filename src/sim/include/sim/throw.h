#pragma once

#include <cstdint>

#include "common/player_input.h"
#include "sim/arm_properties.h"
#include "sim/arm_state.h"
#include "sim/ball_properties.h"
#include "sim/grip.h"
#include "sim/state.h"

// Throwing: "The throw code consumes a ReleaseState struct (hand
// velocity, grip, wrist and finger inputs, release tick) that a human
// input and a CPU planner both fill." Human input fills it via
// release_state_from_arm (this file); milestone 6's CPU planner will fill
// the same struct its own way and can call throw_ball with it unchanged.
namespace sim {

struct ReleaseState {
    Vec3 hand_velocity;                  // m/s, world frame, BEFORE mass/size effort scaling
    Vec3 release_point;                  // world position
    Vec3 controller_angular_velocity;    // rad/s, raw -- input to the grip's spin matrix
    Vec3 wrist_snap_axis;                // forearm-axis component of hand angular velocity
    double stick_x = 0.0;                // [-1, 1], offsets the finger contact point
    double stick_y = 0.0;
    int grip_preset_index = 0;
    std::uint32_t release_tick = 0;
};

// Which grip preset index common::InputButton's kGripPreset1-4 bits
// select (0 if none are set -- the default/four-seam grip).
int grip_index_from_buttons(std::uint16_t buttons);

// Fills a ReleaseState from the pitcher's current kinematic ArmState and
// PlayerInput, evaluated at the release tick. Pure function of its inputs.
ReleaseState release_state_from_arm(const ArmState& arm, const common::PlayerInput& input, const ArmProperties& arm_props,
                                     std::uint32_t release_tick);

// Launches the ball from a ReleaseState: "Release speed = arm angular
// velocity x virtual arm length" (already folded into hand_velocity by
// the caller), scaled for the actual ball's mass ("v = v_measured *
// sqrt((M_arm + m_ref) / (M_arm + m))") and the grip's speed penalty;
// spin from the grip's controller-angular-velocity-to-spin matrix, blended
// with the wrist-snap axis, plus finger-impulse spin ("Spin = finger
// impulse x r / I", r = the stick-offset finger contact point).
BallState throw_ball(const ReleaseState& release, const ArmProperties& arm_props, const BallProperties& ball_props,
                      const GripPreset& grip);

}  // namespace sim
