#pragma once

#include "sim/state.h"

// "The Milestone 4 pitching machine is the simplest CPU pitcher": a pure
// function that produces a BallState for a pitch aimed at a target point
// with a given speed and spin, with no arm model, no release timing, and
// no human/CPU decision-making -- those arrive with the real arm model
// (milestone 5) and CPU pitcher (milestone 6), which can both eventually
// call something like this as their final "launch the ball" step.
namespace sim {

// Produces a ball state at the pitching machine's release point, velocity
// aimed from release_point toward target_point at speed_mps, with the
// given spin (rad/s, world frame) and zero initial angular displacement.
BallState pitching_machine_launch(const Vec3& release_point, const Vec3& target_point, double speed_mps,
                                   const Vec3& spin_rad_s);

}  // namespace sim
