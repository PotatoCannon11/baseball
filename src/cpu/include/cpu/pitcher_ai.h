#pragma once

#include <cstdint>

#include "common/player_input.h"
#include "sim/arm_properties.h"
#include "sim/rng.h"
#include "sim/state.h"

// "The Milestone 4 pitching machine is the simplest CPU pitcher" --
// PitcherAi is the real one: it drives the SAME PlayerInput -> ArmState ->
// throw pipeline a human controller would (sim::step never knows the
// difference), by solving for the arm angular velocity that gives the
// desired release hand_velocity via the existing omega x lever relation,
// holding the throw button through a scripted windup, then releasing on
// schedule.
namespace cpu {

struct PitcherDifficulty {
    double min_speed_mps = 33.0;   // ~74 mph
    double max_speed_mps = 42.0;   // ~94 mph
    double location_jitter_m = 0.12;  // radius of aim error around the target zone center
    std::uint32_t windup_ticks = 40;  // ticks the throw button is held before release (~167 ms at 240 Hz)
    double grip_weights[4] = {0.55, 0.15, 0.20, 0.10};  // probability of choosing each grip preset
};

struct PitcherBrain {
    bool winding_up = false;
    std::uint32_t release_tick = 0;
    sim::Vec3 target_point;
    double speed_mps = 0.0;
    int grip_index = 0;
    sim::Vec3 omega;  // solved once at windup start, held constant through the windup
};

// Called by the at-bat loop when it wants the pitcher to begin a new
// pitch (brain->winding_up must be false). Picks a target within the
// strike-zone box (plus jitter, so not every pitch is a strike), a speed,
// and a grip, and solves the constant angular velocity that -- given the
// fixed identity-orientation arm convention -- makes
// omega x lever == desired hand_velocity direction/magnitude.
void begin_pitch(const sim::Vec3& release_point, const sim::Vec3& zone_center, const sim::Vec3& zone_half_extent,
                  const PitcherDifficulty& difficulty, const sim::ArmProperties& arm_props, std::uint32_t current_tick,
                  sim::RngState* rng, PitcherBrain* brain);

// Called every tick while brain->winding_up is true (including the
// release tick itself). Returns the pitcher's PlayerInput for this tick.
common::PlayerInput tick_pitch(std::uint32_t current_tick, PitcherBrain* brain);

}  // namespace cpu
