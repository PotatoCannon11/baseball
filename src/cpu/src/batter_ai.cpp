#include "cpu/batter_ai.h"

#include <algorithm>
#include <cmath>

#include "sim/math.h"

namespace cpu {

namespace {
constexpr double kReadyTheta = -1.0;     // radians, "cocked back" idle stance (see header)
constexpr double kContactRadius = 0.75;  // matches the barrel region used throughout milestone 4/5 tests
}  // namespace

void reset_for_new_pitch(BatterBrain* brain) {
    brain->decided = false;
    brain->swinging = false;
}

void maybe_decide(const sim::SimState& state, std::uint32_t pitch_release_tick, double tick_dt_seconds,
                   const sim::Vec3& zone_center, const sim::Vec3& zone_half_extent,
                   const BatterDifficulty& difficulty, sim::RngState* rng_state, BatterBrain* brain) {
    if (brain->decided) return;

    const double elapsed_s = static_cast<double>(state.tick - pitch_release_tick) * tick_dt_seconds;
    if (elapsed_s < difficulty.reaction_delay_s) return;
    if (state.ball.velocity.z >= 0.0) return;  // not headed toward the plate (or already past it)

    sim::Rng rng(rng_state);

    // Imperfect "read": straight-line extrapolation from the ball's
    // CURRENT position/velocity to z = zone_center.z, ignoring drag/
    // gravity curvature -- a real batter's early read is an estimate, not
    // a physics solve, and this alone produces occasional swings at
    // pitches that end up outside the zone (or takes on ones that would
    // have been strikes) even before any explicit timing jitter.
    const double t_to_plate = (zone_center.z - state.ball.position.z) / state.ball.velocity.z;
    const sim::Vec3 predicted = state.ball.position + state.ball.velocity * t_to_plate;

    const bool in_zone = std::fabs(predicted.x - zone_center.x) <= zone_half_extent.x &&
                          std::fabs(predicted.y - zone_center.y) <= zone_half_extent.y;
    const double swing_probability =
        in_zone ? difficulty.swing_probability_in_zone : difficulty.swing_probability_out_of_zone;

    brain->decided = true;
    brain->swinging = rng.next_double() < swing_probability;
    if (!brain->swinging) return;

    // Box-Muller for a roughly Gaussian timing error (ticks); a uniform
    // jitter would under-represent "mostly on time, occasionally way
    // off," which is what makes contact quality vary realistically.
    const double u1 = std::max(1e-12, rng.next_double());
    const double u2 = rng.next_double();
    const double gaussian = sim::math::sqrt(-2.0 * sim::math::log(u1)) * sim::math::cos(sim::math::kTwoPi * u2);
    const double timing_error_ticks = gaussian * difficulty.timing_error_stdev_ticks;

    const std::uint32_t ideal_contact_tick =
        state.tick + static_cast<std::uint32_t>(std::max(0.0, t_to_plate / tick_dt_seconds));
    const double contact_tick_signed = static_cast<double>(ideal_contact_tick) + timing_error_ticks;
    brain->contact_tick = static_cast<std::uint32_t>(std::max(0.0, contact_tick_signed));

    brain->omega_magnitude = difficulty.bat_speed_mps / kContactRadius;
    // Ticks needed to sweep theta from kReadyTheta up to 0 (the plate) at
    // that angular rate, so the swing arrives on schedule regardless of
    // how fast this particular swing is.
    const double arc_seconds = (0.0 - kReadyTheta) / brain->omega_magnitude;
    const auto arc_ticks = static_cast<std::uint32_t>(std::max(1.0, arc_seconds / tick_dt_seconds));
    brain->swing_start_tick = brain->contact_tick > arc_ticks ? brain->contact_tick - arc_ticks : 0;
}

common::PlayerInput tick_bat(std::uint32_t current_tick, const BatterBrain& brain, double tick_dt_seconds) {
    common::PlayerInput input{};
    input.tick = current_tick;

    double theta = kReadyTheta;
    double omega_x = 0.0;
    const bool sweeping = brain.swinging && current_tick >= brain.swing_start_tick;
    if (sweeping) {
        omega_x = brain.omega_magnitude;
        const std::uint32_t ticks_elapsed = current_tick - brain.swing_start_tick;
        theta = std::min(1.0, kReadyTheta + omega_x * (static_cast<double>(ticks_elapsed) * tick_dt_seconds));
    }

    const sim::Quat orientation = sim::Quat{sim::math::cos(theta * 0.5), sim::math::sin(theta * 0.5), 0.0, 0.0};

    const double quat_scale = common::PlayerInput::kQuatScale;
    input.orientation[0] = static_cast<std::int16_t>(std::clamp(orientation.x * quat_scale, -32768.0, 32767.0));
    input.orientation[1] = static_cast<std::int16_t>(std::clamp(orientation.y * quat_scale, -32768.0, 32767.0));
    input.orientation[2] = static_cast<std::int16_t>(std::clamp(orientation.z * quat_scale, -32768.0, 32767.0));
    input.orientation[3] = static_cast<std::int16_t>(std::clamp(orientation.w * quat_scale, -32768.0, 32767.0));

    const double ang_scale = common::PlayerInput::kAngVelScale;
    input.angular_velocity[0] = static_cast<std::int16_t>(std::clamp(omega_x * ang_scale, -32768.0, 32767.0));
    input.angular_velocity[1] = 0;
    input.angular_velocity[2] = 0;

    if (brain.swinging) input.buttons = static_cast<std::uint16_t>(common::InputButton::kSwing);

    return input;
}

}  // namespace cpu
