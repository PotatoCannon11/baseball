#include "cpu/pitcher_ai.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cpu {

namespace {

int choose_grip(const double weights[4], sim::Rng& rng) {
    double total = 0.0;
    for (int i = 0; i < 4; ++i) total += weights[i];
    double roll = rng.next_range(0.0, total > 0.0 ? total : 1.0);
    for (int i = 0; i < 4; ++i) {
        roll -= weights[i];
        if (roll <= 0.0) return i;
    }
    return 3;
}

// Ballistic targeting in the vertical plane containing release->target:
// given horizontal distance d, required rise/drop dy, and launch speed v,
// solve the launch angle theta so a (drag-free) projectile passes through
// the target. Standard projectile-targeting quadratic in u = tan(theta):
//   dy = d*u - (g*d^2/(2*v^2))*(1+u^2)
// This ignores drag/Magnus for the AIMING solve only (the actual pitch
// still flies under full sim physics) -- without it, a straight-line aim
// undershoots the target by the flight's gravity drop, which over a ~18 m
// pitch at these speeds is close to a meter, easily missing the strike
// zone entirely. Picks the flatter ("fastball-like") of the two
// solutions. Returns false (falls back to a straight-line aim) if the
// target is unreachable at this speed.
bool solve_launch_angle(double horizontal_distance, double rise, double speed_mps, double gravity_mps2,
                         double* out_theta) {
    if (horizontal_distance < 1e-6 || speed_mps < 1e-6) return false;
    const double a = (gravity_mps2 * horizontal_distance * horizontal_distance) / (2.0 * speed_mps * speed_mps);
    const double b = -horizontal_distance;
    const double c = rise + a;
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0 || a < 1e-12) return false;
    const double sqrt_disc = std::sqrt(discriminant);
    const double u_flat = (-b - sqrt_disc) / (2.0 * a);  // flatter of the two roots
    *out_theta = std::atan(u_flat);
    return true;
}

}  // namespace

void begin_pitch(const sim::Vec3& release_point, const sim::Vec3& zone_center, const sim::Vec3& zone_half_extent,
                  const PitcherDifficulty& difficulty, const sim::ArmProperties& arm_props, std::uint32_t current_tick,
                  sim::RngState* rng_state, PitcherBrain* brain) {
    sim::Rng rng(rng_state);

    const sim::Vec3 jitter{
        rng.next_range(-difficulty.location_jitter_m, difficulty.location_jitter_m),
        rng.next_range(-difficulty.location_jitter_m, difficulty.location_jitter_m),
        0.0,
    };
    brain->target_point = sim::Vec3{
        zone_center.x + std::clamp(jitter.x, -zone_half_extent.x * 1.6, zone_half_extent.x * 1.6),
        zone_center.y + std::clamp(jitter.y, -zone_half_extent.y * 1.6, zone_half_extent.y * 1.6),
        zone_center.z,
    };
    brain->speed_mps = rng.next_range(difficulty.min_speed_mps, difficulty.max_speed_mps);
    brain->grip_index = choose_grip(difficulty.grip_weights, rng);
    brain->release_tick = current_tick + difficulty.windup_ticks;
    brain->winding_up = true;

    // Aim direction: ballistic-compensated for gravity drop (see
    // solve_launch_angle) rather than a straight line at the target,
    // which would land the pitch well under the strike zone over an
    // 18 m throw.
    const sim::Vec3 horizontal_delta{brain->target_point.x - release_point.x, 0.0,
                                      brain->target_point.z - release_point.z};
    const double horizontal_distance = horizontal_delta.length();
    const sim::Vec3 horizontal_unit =
        horizontal_distance > 1e-9 ? horizontal_delta * (1.0 / horizontal_distance) : sim::Vec3{0.0, 0.0, -1.0};
    const double rise = brain->target_point.y - release_point.y;

    double theta = 0.0;
    sim::Vec3 direction;
    const bool solved = solve_launch_angle(horizontal_distance, rise, brain->speed_mps, 9.80665, &theta);
    if (solved) {
        direction = (horizontal_unit * std::cos(theta)) + (sim::Vec3{0.0, 1.0, 0.0} * std::sin(theta));
    } else {
        direction = (brain->target_point - release_point).normalized();  // fallback: straight line
    }
    const sim::Vec3 desired_hand_velocity = direction * brain->speed_mps;
    const double lever_length = arm_props.virtual_arm_length_m;
    brain->omega = sim::Vec3{
        lever_length > 1e-9 ? desired_hand_velocity.z / lever_length : 0.0,
        0.0,
        lever_length > 1e-9 ? -desired_hand_velocity.x / lever_length : 0.0,
    };
#ifdef BASEBALL_CPU_VS_CPU_DEBUG
    std::fprintf(stderr,
                 "begin_pitch: grip=%d d=%.3f rise=%.3f speed=%.3f theta=%.4f target=(%.3f,%.3f,%.3f) omega=(%.3f,%.3f,%.3f)\n",
                 brain->grip_index, horizontal_distance, rise, brain->speed_mps, theta, brain->target_point.x,
                 brain->target_point.y, brain->target_point.z, brain->omega.x, brain->omega.y, brain->omega.z);
#endif
}

common::PlayerInput tick_pitch(std::uint32_t current_tick, PitcherBrain* brain) {
    common::PlayerInput input{};
    input.tick = current_tick;
    // Identity orientation -- see begin_pitch's omega-solving comment;
    // the whole pitch is modeled as a fixed-orientation arm sweeping at a
    // constant angular velocity, not a rotating one.
    input.orientation[3] = static_cast<std::int16_t>(common::PlayerInput::kQuatScale);

    std::int16_t omega_i16[3];
    const double s = common::PlayerInput::kAngVelScale;
    omega_i16[0] = static_cast<std::int16_t>(std::clamp(brain->omega.x * s, -32768.0, 32767.0));
    omega_i16[1] = static_cast<std::int16_t>(std::clamp(brain->omega.y * s, -32768.0, 32767.0));
    omega_i16[2] = static_cast<std::int16_t>(std::clamp(brain->omega.z * s, -32768.0, 32767.0));
    input.angular_velocity[0] = omega_i16[0];
    input.angular_velocity[1] = omega_i16[1];
    input.angular_velocity[2] = omega_i16[2];

    const bool release_now = current_tick >= brain->release_tick;
    if (!release_now) {
        input.buttons = static_cast<std::uint16_t>(common::InputButton::kThrow);
    } else {
        brain->winding_up = false;  // this was the release tick; done
    }

    switch (brain->grip_index) {
        case 1:
            input.buttons |= static_cast<std::uint16_t>(common::InputButton::kGripPreset2);
            break;
        case 2:
            input.buttons |= static_cast<std::uint16_t>(common::InputButton::kGripPreset3);
            break;
        case 3:
            input.buttons |= static_cast<std::uint16_t>(common::InputButton::kGripPreset4);
            break;
        default:
            break;  // grip 0: no preset button, the default
    }

    return input;
}

}  // namespace cpu
