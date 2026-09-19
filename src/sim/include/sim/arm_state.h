#pragma once

#include <cstdint>

#include "sim/quat.h"
#include "sim/vec3.h"

// The pitcher's throwing arm: kinematic (driven directly by PlayerInput,
// exactly like BatState), plus the one bit of history sim::step() needs
// that a single tick's PlayerInput can't provide on its own -- last
// tick's button state, to detect the release edge ("a shoulder-trigger
// button-up event, looked up at its timestamp").
namespace sim {

struct ArmState {
    Vec3 pivot_position;           // meters, world frame -- the shoulder
    Quat orientation;
    Vec3 angular_velocity;         // rad/s
    std::uint16_t prev_buttons = 0;
};

}  // namespace sim
