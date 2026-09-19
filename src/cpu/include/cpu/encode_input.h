#pragma once

#include <algorithm>
#include <cstdint>

#include "common/player_input.h"
#include "sim/quat.h"
#include "sim/vec3.h"

// The sim only ever sees common::PlayerInput -- "It cannot tell the
// sources apart." A CPU player is just another PlayerInput producer, so
// it needs the same quantization the input layer uses when it packages a
// real device's orientation/angular-velocity/stick into that struct. Kept
// here (not in sim, which must stay input-agnostic, and not depending on
// the input library, which is device/SDL-oriented) since it's the
// smallest reasonable home for "a CPU-only encoder."
namespace cpu {

inline std::int16_t quantize(double value, double scale) {
    const double clamped = std::clamp(value, -1.0, 1.0);
    return static_cast<std::int16_t>(std::clamp(clamped * scale, -32768.0, 32767.0));
}

inline void encode_orientation(const sim::Quat& q, std::int16_t out[4]) {
    const sim::Quat n = q.normalized();
    const double s = common::PlayerInput::kQuatScale;
    out[0] = quantize(n.x, s);
    out[1] = quantize(n.y, s);
    out[2] = quantize(n.z, s);
    out[3] = quantize(n.w, s);
}

inline void encode_angular_velocity(const sim::Vec3& omega, std::int16_t out[3]) {
    const double s = common::PlayerInput::kAngVelScale;
    out[0] = static_cast<std::int16_t>(std::clamp(omega.x * s, -32768.0, 32767.0));
    out[1] = static_cast<std::int16_t>(std::clamp(omega.y * s, -32768.0, 32767.0));
    out[2] = static_cast<std::int16_t>(std::clamp(omega.z * s, -32768.0, 32767.0));
}

}  // namespace cpu
