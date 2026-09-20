#pragma once

#include "common/player_input.h"
#include "sim/config.h"
#include "sim/state.h"

// The sim's entire public surface for advancing time: next_state =
// step(state, inputs, config). Pure function, no clock reads, no
// filesystem, no allocation, no globals -- callable identically on any
// peer given the same three inputs (this is what later milestones'
// resimulation/rollback and CPU-vs-CPU headless batches both lean on).
namespace sim {

struct SimInputs {
    common::PlayerInput pitcher;
    common::PlayerInput batter;
};

// Optional per-tick event summary -- e.g. milestone 6's at-bat loop needs
// to know whether a swing actually made bat-ball contact (vs. a swing and
// a miss), which step()'s SimState return value alone doesn't expose.
// Purely additive: defaults to nullptr, so every earlier call site is
// unaffected.
struct StepEvents {
    bool bat_contact_occurred = false;
    bool ball_released_this_tick = false;
    // Edge-triggered: fires exactly on the tick both roles' kReady bits
    // have been seen (or the config's ready timeout elapses), then the
    // wait window immediately restarts. The driver (versus join-flow /
    // CPU-vs-CPU loop) uses this as the deterministic signal for "begin
    // the next pitch," rather than polling SimState::ready every tick.
    bool ready_for_next_pitch = false;
};

// Milestone 4 physics: RK4 free flight (gravity, drag, Magnus, spin decay,
// added mass/buoyancy), continuous collision against the bat (a capsule
// chain built from the same profile the renderer laths) and the ground
// plane, with a Hunt-Crossley compliant contact model. The bat pose is
// decoded directly from the batter's PlayerInput each tick -- the bat is
// purely kinematic, never moved by the contact code, per spec.
//
// Milestone 5 adds throwing: the pitcher's arm is decoded the same
// kinematic way, and a release (shoulder-trigger button-up, looked up at
// its own tick) launches the ball via the arm/grip/finger model in
// sim/throw.h, overriding whatever the ball was doing that tick.
SimState step(const SimState& prev, const SimInputs& inputs, const SimConfig& config, StepEvents* events = nullptr);

}  // namespace sim
