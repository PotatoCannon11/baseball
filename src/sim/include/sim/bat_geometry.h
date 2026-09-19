#pragma once

#include "sim/bat_properties.h"
#include "sim/state.h"

// Builds the bat's world-space collision chain from the SAME profile data
// the renderer laths its mesh from -- "collision (no raycast movement) ...
// test the ball center's swept segment against the bat as a chain of
// capsules/cones inflated by ball radius." A capsule here is a line
// segment with a (possibly different) radius at each end; the true offset
// surface of a cone frustum isn't quite a swept sphere along its axis, but
// it's a standard, cheap, well-behaved approximation for a tapered rod and
// is exact at both end caps.
namespace sim {

struct Capsule {
    Vec3 a, b;
    double radius_a = 0.0, radius_b = 0.0;
};

inline constexpr int kMaxBatCapsules = kMaxBatProfilePoints - 1;

struct BatCapsuleChain {
    Capsule segments[kMaxBatCapsules];
    int count = 0;
};

// bat.pivot_position is the knob (profile y=0); bat.orientation rotates the
// profile's local +Y axis (knob -> tip) into world space.
inline BatCapsuleChain build_bat_capsule_chain(const BatProperties& props, const BatState& bat) {
    BatCapsuleChain chain;
    chain.count = props.profile_count - 1;
    for (int i = 0; i < chain.count; ++i) {
        const Vec3 local_a{0.0, props.profile[i].y, 0.0};
        const Vec3 local_b{0.0, props.profile[i + 1].y, 0.0};
        chain.segments[i] = Capsule{
            bat.pivot_position + bat.orientation.rotate(local_a),
            bat.pivot_position + bat.orientation.rotate(local_b),
            props.profile[i].radius,
            props.profile[i + 1].radius,
        };
    }
    return chain;
}

}  // namespace sim
