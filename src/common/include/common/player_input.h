#pragma once

#include <cstdint>
#include <type_traits>

// PlayerInput is the contract between the input layer and the (not yet
// built, milestone 3) sim: "the sim only ever sees PlayerInput. It cannot
// tell the sources apart." Local(device), Cpu(profile), and Scripted
// (recorded file) sources all produce the exact same struct.
//
// Fixed-size, versioned, trivially copyable POD -- safe to memcpy, record
// to a file, and later (once a real wire format exists) serialize
// explicitly field by field. Orientation and angular velocity are
// quantized to fixed-point integers rather than stored as float: a
// recorded PlayerInput stream is meant to become a regression test, and a
// stable integer binary layout doesn't depend on the replaying compiler's
// float rounding the same way the recording compiler's did.
namespace common {

enum class InputButton : std::uint16_t {
    kSwing = 1u << 0,
    kThrow = 1u << 1,        // shoulder-trigger release, per spec
    kGripPreset1 = 1u << 2,
    kGripPreset2 = 1u << 3,
    kGripPreset3 = 1u << 4,
    kGripPreset4 = 1u << 5,
    kRecenter = 1u << 6,
    kReady = 1u << 7,
};

struct PlayerInput {
    static constexpr std::uint16_t kVersion = 1;

    // Quaternion component scale: components are in [-1, 1], scaled to fill
    // an int16.
    static constexpr float kQuatScale = 32767.0f;
    // Angular velocity scale: the documented Joy-Con gyro range is
    // +/-2000 deg/s ~= +/-34.9 rad/s (see src/input/include/input/
    // device_profile.h for the caveat on that number). This scale keeps
    // that full range inside int16 with headroom for calibration overshoot.
    static constexpr float kAngVelScale = 900.0f;
    static constexpr float kStickScale = 32767.0f;

    std::uint16_t version = kVersion;
    std::uint16_t sequence = 0;  // monotonically increasing per role
    std::uint32_t tick = 0;      // sim tick this input targets

    // Fused bat/arm orientation quaternion, x,y,z,w, quantized by kQuatScale.
    std::int16_t orientation[4] = {0, 0, 0, 32767};
    // Angular velocity in rad/s, x,y,z, quantized by kAngVelScale.
    std::int16_t angular_velocity[3] = {0, 0, 0};

    std::int16_t stick_x = 0;
    std::int16_t stick_y = 0;

    std::uint16_t buttons = 0;    // InputButton bitmask
    std::uint8_t clip_flags = 0;  // bit per sensor axis that clipped this sample
    std::uint8_t reserved = 0;    // padding today; keeps struct size stable if used
};

static_assert(sizeof(PlayerInput) == 32,
              "PlayerInput layout changed -- bump kVersion and check recorder/replay compatibility");
static_assert(std::is_trivially_copyable_v<PlayerInput>, "PlayerInput must stay memcpy-safe");

}  // namespace common
