#pragma once

#include "sim/ball_properties.h"
#include "sim/environment.h"
#include "sim/state.h"

// Free-flight ball integration: gravity, drag, Magnus, added mass and
// buoyancy, integrated with RK4 in double, per spec. No ground/bat contact
// here -- this is pure aerodynamics, called by sim::step() for whatever
// fraction of a tick the ball spends NOT in contact with something.
namespace sim {

// Advances (position, velocity) by dt seconds of free flight. Spin
// (angular_velocity) decays exponentially (domega/dt = -omega/tau) and is
// applied via its closed-form solution rather than folded into the RK4
// state vector, since tau (seconds) >> dt (~4 ms at 240 Hz) makes "spin
// held constant across one step's RK4 stages" an error far smaller than
// other modeling approximations already present (Cd/Cl fits). Orientation
// just integrates whatever angular velocity results, as before.
BallState integrate_flight_rk4(const BallState& prev, const BallProperties& ball, const Environment& env,
                                double gravity_mps2, double dt);

}  // namespace sim
