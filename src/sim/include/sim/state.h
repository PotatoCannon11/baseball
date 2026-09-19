#pragma once

#include <cstdint>
#include <type_traits>

#include "sim/arm_state.h"
#include "sim/quat.h"
#include "sim/rng.h"
#include "sim/vec3.h"

// All sim state lives in ONE POD, trivially copyable, fixed-size struct,
// no pointers (indices only), per the architecture spec. This is
// milestone 3's minimal "bare" version: ball and bat kinematics enough to
// drive a renderer and prove the tick/snapshot/hash/determinism
// machinery. At-bat state and CPU brain state are added when milestone 6
// actually implements the at-bat loop and CPU players -- there is no
// value in preallocating fields for behavior that doesn't exist yet.
//
// IMPORTANT: always value-initialize (SimState{}), never default-construct
// (SimState s;) without braces. Struct padding between members is only
// guaranteed zeroed by value-initialization; sim::hash_state() hashes
// named fields individually specifically so padding bytes can never leak
// into the hash, but other raw-byte uses (snapshot memcpy) still benefit
// from a fully deterministic byte image.
namespace sim {

struct BallState {
    Vec3 position;           // meters, world frame
    Vec3 velocity;           // m/s
    Quat orientation;
    Vec3 angular_velocity;   // rad/s
};

struct BatState {
    Vec3 pivot_position;     // meters, world frame -- the virtual pivot, not the barrel
    Quat orientation;
    Vec3 angular_velocity;   // rad/s
};

struct SimState {
    std::uint32_t tick = 0;
    BallState ball;
    BatState bat;
    ArmState pitcher_arm;
    RngState rng;
};

static_assert(std::is_trivially_copyable_v<SimState>, "SimState must stay memcpy-safe (snapshots are memcpy)");

}  // namespace sim
