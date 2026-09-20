#pragma once

#include "versus/role_assignment.h"

// Milestone 7 config: "Optional 'tells' (windup animation differences per
// grip) are a config flag for difficulty and default to off, so a human
// batter isn't given the pitch type for free."
//
// NOTE on tells_enabled: there is currently no per-grip windup animation
// content at all (the pitcher's arm is one kinematic model with no
// grip-dependent visual variation -- see sim/step.cpp's step_arm_from_
// input), so this flag has nothing to switch on yet. It's wired through
// now (default off, per spec) so the eventual animation work has a place
// to plug in rather than needing a config-layer change later; flipping it
// on today would be a no-op, which is honestly documented here rather
// than silently pretended to work.
namespace versus {

struct VersusConfig {
    RoleScheduleConfig role_schedule;
    bool tells_enabled = false;
};

}  // namespace versus
