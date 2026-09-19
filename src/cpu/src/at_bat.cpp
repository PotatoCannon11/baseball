#include "cpu/at_bat.h"

#include <cmath>

#include "sim/flight.h"

namespace cpu {

bool is_in_zone(const sim::Vec3& position, const StrikeZoneBox& zone) {
    return std::fabs(position.x - zone.center.x) <= zone.half_extent.x &&
           std::fabs(position.y - zone.center.y) <= zone.half_extent.y;
}

bool is_fair_territory(double x, double z) { return z > 0.0 && std::fabs(x) <= z; }

PitchOutcome classify_no_contact(bool swung, bool in_zone) {
    if (swung) return PitchOutcome::SwingingStrike;
    return in_zone ? PitchOutcome::CalledStrike : PitchOutcome::CalledBall;
}

PitchOutcome classify_contact(const sim::BallState& ball_at_contact, const sim::SimConfig& config,
                               FairBallResult* out_fair_result) {
    const double exit_velocity = ball_at_contact.velocity.length();
    const double horizontal_speed =
        std::sqrt(ball_at_contact.velocity.x * ball_at_contact.velocity.x +
                  ball_at_contact.velocity.z * ball_at_contact.velocity.z);
    const double launch_angle_rad = std::atan2(ball_at_contact.velocity.y, horizontal_speed);

    // Integrate flight forward (same code the sim itself uses) until the
    // ball lands, to find where it first touches the ground -- the
    // simplified fair/foul determination point for this sandbox (real
    // baseball also cares about a ball going foul after passing a base,
    // out of scope here).
    sim::BallState ball = ball_at_contact;
    const double dt = config.tick_dt_seconds;
    for (int i = 0; i < 240 * 30; ++i) {  // 30 s ceiling, generous for any realistic hit
        const sim::BallState next = sim::integrate_flight_rk4(ball, config.ball, config.environment,
                                                                config.gravity_mps2, dt);
        if (next.position.y - config.ball.radius_m <= 0.0 && ball.position.y - config.ball.radius_m > 0.0) {
            const double denom = ball.position.y - next.position.y;
            const double t_frac = denom > 1e-12 ? ball.position.y / denom : 0.0;
            const double land_x = ball.position.x + (next.position.x - ball.position.x) * t_frac;
            const double land_z = ball.position.z + (next.position.z - ball.position.z) * t_frac;

            if (out_fair_result) {
                out_fair_result->exit_velocity_mps = exit_velocity;
                out_fair_result->launch_angle_deg = launch_angle_rad * (180.0 / sim::math::kPi);
                out_fair_result->carry_distance_m = std::sqrt(land_x * land_x + land_z * land_z);
            }
            return is_fair_territory(land_x, land_z) ? PitchOutcome::FairBall : PitchOutcome::Foul;
        }
        ball = next;
    }
    // Never landed within the ceiling (shouldn't happen for any
    // physically plausible hit) -- treat conservatively as foul rather
    // than silently reporting a bogus fair-ball distance.
    return PitchOutcome::Foul;
}

}  // namespace cpu
