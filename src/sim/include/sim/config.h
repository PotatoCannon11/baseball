#pragma once

#include <cstdint>

#include "sim/arm_properties.h"
#include "sim/ball_properties.h"
#include "sim/bat_properties.h"
#include "sim/environment.h"
#include "sim/grip.h"

// Ball/bat/grip property config, immutable during a tick, versioned and
// hashed, agreed at session start -- separate from SimState per the
// architecture spec: "Live edits from the property editor become
// timestamped sim commands applied at a specified tick, never direct
// mutation." Milestone 4 adds the real ball/bat/environment property
// structs; grip/CPU-difficulty/per-player-calibration structs arrive when
// milestone 5/6/7 need them (config is additive across milestones, same
// as SimState).
namespace sim {

struct SimConfig {
    std::uint32_t version = 1;

    // 240 Hz: see tools/sim_bench, which measures actual step() cost on
    // this machine to justify the choice per the spec's "choose and
    // justify from measured step cost." 240 Hz gives a ~4.17 ms/tick
    // budget; milestone 4's real collision/flight code re-measures this
    // (tools/sim_headless --bench).
    double tick_dt_seconds = 1.0 / 240.0;
    double gravity_mps2 = 9.80665;

    BallProperties ball;
    BatProperties bat;
    Environment environment;
    GroundSurface ground = ground_presets::kGrass;
    ArmProperties arm;
    GripTable grips;
};

}  // namespace sim
