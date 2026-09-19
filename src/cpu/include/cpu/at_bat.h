#pragma once

#include "sim/config.h"
#include "sim/state.h"

// At-bat classification: "Pitch, swing or take, then classify: called
// ball, called strike, swinging strike, foul (simple 90-degree
// fair-territory wedge from home plate), or fair ball with distance,
// exit velocity, and launch angle. No fielders, baserunning, or innings."
namespace cpu {

// The plate is at z=0 in this codebase's convention (pitches travel from
// the mound at z~18 toward z=0; see sim/step.cpp's arm pivot placeholder).
struct StrikeZoneBox {
    sim::Vec3 center{0.0, 0.85, 0.0};      // roughly belt height for an average batter
    sim::Vec3 half_extent{0.22, 0.40, 0.0};  // ~17in wide plate, ~knees-to-chest height
};

enum class PitchOutcome {
    InProgress,
    CalledBall,
    CalledStrike,
    SwingingStrike,
    Foul,
    FairBall,
};

struct FairBallResult {
    double exit_velocity_mps = 0.0;
    double launch_angle_deg = 0.0;
    double carry_distance_m = 0.0;
};

bool is_in_zone(const sim::Vec3& position, const StrikeZoneBox& zone);

// "Simple 90-degree fair-territory wedge from home plate": fair territory
// is z > 0 (back toward where the pitch came from -- see the header
// comment) and |x| <= z (within 45 degrees of the center line either
// side).
bool is_fair_territory(double x, double z);

// Classifies a pitch that reached/passed the plate WITHOUT any bat
// contact -- either the batter took it, or swung and missed.
PitchOutcome classify_no_contact(bool swung, bool in_zone);

// Classifies a bat-ball contact result (fair or foul) and, if fair,
// integrates the flight forward (reusing the same flight/ground-contact
// code the sim itself uses) until it lands, reporting exit velocity,
// launch angle, and carry distance.
PitchOutcome classify_contact(const sim::BallState& ball_at_contact, const sim::SimConfig& config,
                               FairBallResult* out_fair_result);

}  // namespace cpu
