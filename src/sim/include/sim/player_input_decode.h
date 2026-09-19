#pragma once

#include "common/player_input.h"
#include "sim/quat.h"
#include "sim/vec3.h"

// The sim's only view into player intent is common::PlayerInput (the
// input layer's already-quantized, float-based output). These decode its
// fixed-point fields back into the double Vec3/Quat types the sim uses
// internally. The sim never touches raw sensor data, SDL, or any input
// device directly.
namespace sim {

inline Quat decode_orientation(const common::PlayerInput& p) {
    const double inv_scale = 1.0 / static_cast<double>(common::PlayerInput::kQuatScale);
    // PlayerInput stores (x, y, z, w); sim::Quat is (w, x, y, z).
    return Quat{
        static_cast<double>(p.orientation[3]) * inv_scale,
        static_cast<double>(p.orientation[0]) * inv_scale,
        static_cast<double>(p.orientation[1]) * inv_scale,
        static_cast<double>(p.orientation[2]) * inv_scale,
    }
        .normalized();
}

inline Vec3 decode_angular_velocity(const common::PlayerInput& p) {
    const double inv_scale = 1.0 / static_cast<double>(common::PlayerInput::kAngVelScale);
    return Vec3{
        static_cast<double>(p.angular_velocity[0]) * inv_scale,
        static_cast<double>(p.angular_velocity[1]) * inv_scale,
        static_cast<double>(p.angular_velocity[2]) * inv_scale,
    };
}

}  // namespace sim
