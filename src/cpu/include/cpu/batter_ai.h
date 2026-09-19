#pragma once

#include <cstdint>

#include "common/player_input.h"
#include "sim/state.h"

// CPU batter: watches the pitch, decides swing-or-take, and if swinging
// produces a scripted bat orientation/angular-velocity arc (the same
// PlayerInput fields a human's motion controller would fill) timed to
// bring the barrel through the plate around the predicted arrival tick,
// with timing error so contact isn't always solid.
//
// Swing convention (matches the bat/collision tests throughout milestone
// 4/5): the bat pivots about world +Y=0 plane... concretely, orientation
// is a rotation about world +X by angle theta, so the barrel sweeps in
// the Y-Z plane at a fixed local radius from the pivot (0,1,0). theta=0
// puts the barrel roughly over the plate (z=0) at strike-zone height;
// negative theta is "cocked back" (barrel behind and below the plate,
// clear of an oncoming pitch) -- the ready/idle stance.
namespace cpu {

struct BatterDifficulty {
    double swing_probability_in_zone = 0.75;
    double swing_probability_out_of_zone = 0.12;
    double timing_error_stdev_ticks = 3.0;  // ~12.5 ms at 240 Hz
    double bat_speed_mps = 31.29;           // ~70 mph at the contact radius
    double reaction_delay_s = 0.12;         // time after release before the batter can react
};

struct BatterBrain {
    bool decided = false;       // has this pitch been judged swing/take yet
    bool swinging = false;
    std::uint32_t swing_start_tick = 0;
    std::uint32_t contact_tick = 0;
    double omega_magnitude = 0.0;
};

// Resets brain for a new pitch (call once when a new pitch is thrown).
void reset_for_new_pitch(BatterBrain* brain);

// Called every tick while the pitch is in flight and not yet judged.
// zone_center/zone_half_extent describe the strike zone box at z=0 (the
// plate). Once the ball has traveled long enough to react to (per
// reaction_delay_s) and hasn't already been judged, this predicts the
// ball's crossing point via straight-line extrapolation (a deliberately
// imperfect "read" -- it ignores drag/gravity curvature, same as a real
// batter guesses from early flight) and commits to swing or take.
void maybe_decide(const sim::SimState& state, std::uint32_t pitch_release_tick, double tick_dt_seconds,
                   const sim::Vec3& zone_center, const sim::Vec3& zone_half_extent,
                   const BatterDifficulty& difficulty, sim::RngState* rng, BatterBrain* brain);

// Returns this tick's batter PlayerInput given the current brain state.
common::PlayerInput tick_bat(std::uint32_t current_tick, const BatterBrain& brain, double tick_dt_seconds);

}  // namespace cpu
