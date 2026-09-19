#include "sim/step.h"

#include "sim/bat_geometry.h"
#include "sim/collision.h"
#include "sim/flight.h"
#include "sim/player_input_decode.h"
#include "sim/throw.h"

namespace sim {

namespace {

BatState step_bat_from_input(const common::PlayerInput& batter_input) {
    BatState bat;
    // No arm model for the BATTER yet (that's a separate future extension
    // of milestone 5's throwing arm to the swing side), so the pivot is a
    // fixed placeholder position near home plate; only orientation/
    // angular velocity come from the live input. Collision code treats
    // the pivot as stationary (bat_pivot_velocity defaults to zero) to
    // match.
    bat.pivot_position = Vec3{0.0, 1.0, 0.0};
    bat.orientation = decode_orientation(batter_input);
    bat.angular_velocity = decode_angular_velocity(batter_input);
    return bat;
}

ArmState step_arm_from_input(const common::PlayerInput& pitcher_input, std::uint16_t prev_buttons) {
    ArmState arm;
    // Fixed placeholder shoulder position near the pitching rubber
    // (regulation distance is 18.44 m from home plate); no body/stride
    // model yet, same "kinematic, driven by PlayerInput" treatment as the
    // bat.
    arm.pivot_position = Vec3{0.0, 1.8, 18.0};
    arm.orientation = decode_orientation(pitcher_input);
    arm.angular_velocity = decode_angular_velocity(pitcher_input);
    arm.prev_buttons = prev_buttons;
    return arm;
}

}  // namespace

SimState step(const SimState& prev, const SimInputs& inputs, const SimConfig& config, StepEvents* events) {
    if (events) *events = StepEvents{};
    SimState next{};
    next.tick = prev.tick + 1;
    next.bat = step_bat_from_input(inputs.batter);
    next.pitcher_arm = step_arm_from_input(inputs.pitcher, inputs.pitcher.buttons);
    next.rng = prev.rng;  // milestone 4/5 flight/collision/throw code doesn't consume randomness yet

    // "Release is a shoulder-trigger button-up event": kThrow was held
    // last tick and is no longer held this tick.
    using common::InputButton;
    const std::uint16_t kThrowBit = static_cast<std::uint16_t>(InputButton::kThrow);
    const bool was_throwing = (prev.pitcher_arm.prev_buttons & kThrowBit) != 0;
    const bool is_throwing = (inputs.pitcher.buttons & kThrowBit) != 0;
    const bool released_this_tick = was_throwing && !is_throwing;

    if (released_this_tick) {
        // The arm state AT release is this tick's (next.pitcher_arm), not
        // last tick's -- the button-up sample and the arm kinematics it's
        // paired with come from the same PlayerInput.
        const ReleaseState release = release_state_from_arm(next.pitcher_arm, inputs.pitcher, config.arm, next.tick);
        const GripPreset& grip = config.grips.presets[release.grip_preset_index];
        next.ball = throw_ball(release, config.arm, config.ball, grip);
        if (events) events->ball_released_this_tick = true;
        return next;
    }

    const BatCapsuleChain chain = build_bat_capsule_chain(config.bat, next.bat);

    // Broad CCD sweep: does the ball's straight-line path this tick come
    // within contact range of the bat? See time_of_impact_fraction for why
    // a straight-line (not curved) sweep is an acceptable approximation
    // over one ~4 ms tick.
    const Vec3 predicted_end = prev.ball.position + prev.ball.velocity * config.tick_dt_seconds;
    const double s_hit = time_of_impact_fraction(prev.ball.position, predicted_end, config.ball.radius_m, chain);

    bool resolved = false;
    if (s_hit >= 0.0) {
        const double t_impact = s_hit * config.tick_dt_seconds;
        const BallState ball_pre_contact =
            integrate_flight_rk4(prev.ball, config.ball, config.environment, config.gravity_mps2, t_impact);
        const ContactOutcome outcome =
            resolve_bat_contact(ball_pre_contact, chain, next.bat, config.bat, config.ball,
                                 config.tick_dt_seconds - t_impact);
        if (outcome.contact_occurred) {
            next.ball = outcome.ball;
            resolved = true;
            if (events) events->bat_contact_occurred = true;
        }
        // If the fine-grained search never found actual penetration, the
        // straight-line sweep was a false positive (bat orientation/
        // radius curvature the linear estimate missed) -- fall through to
        // ordinary free flight for the whole tick below.
    }

    if (!resolved) {
        const BallState free_flight =
            integrate_flight_rk4(prev.ball, config.ball, config.environment, config.gravity_mps2,
                                  config.tick_dt_seconds);
        next.ball = apply_ground_contact(prev.ball, free_flight, config.ball, config.ground, config.tick_dt_seconds);
    }

    return next;
}

}  // namespace sim
