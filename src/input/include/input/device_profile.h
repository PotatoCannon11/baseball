#pragma once

#include <SDL3/SDL.h>

// Per-device IMU profile table. Spec requirement: "Do not hardcode the
// +-2000 deg/s clip threshold. Read it from the profile." Saturation
// detection (in joycon_source.cpp) and the clip-recovery step both look up
// values here instead of hardcoding a number inline.
//
// IMPORTANT CAVEAT on the numeric ranges: these are NOT from an official
// Nintendo datasheet (Nintendo doesn't publish one). They're the commonly
// cited default operating-mode values from public Joy-Con reverse-
// engineering references (e.g. dekuNukem/Nintendo_Switch_Reverse_
// Engineering), which the open-source HIDAPI Joy-Con driver SDL3 itself
// uses is built on. Treat as documented-but-unverified until a real hard
// swing actually clips a real device and this profile's numbers can be
// cross-checked against tools/imu_probe's clip-count measurement.
//
// IMPORTANT CAVEAT on axis mapping: the project spec calls out that a
// left and right Joy-Con have mirrored physical IMU orientations and that
// this must be handled per-device. Whether SDL3's HIDAPI driver already
// normalizes this for the app (so a "swing forward" always reads the same
// sign regardless of which Joy-Con, or which hand, is used) or leaves the
// raw per-chip orientation exposed is NOT yet verified -- doing so needs a
// real Joy-Con (or pair) to swing in a known direction and read the sign
// of the result. `axis_mapping_verified` is false for every entry below
// until that test has actually been run; until then axis_sign is left as
// identity (no correction applied) rather than guessing a mirror matrix.
struct DeviceProfile {
    const char* label;
    float gyro_range_dps;    // full-scale range; clip threshold is +/- this
    float accel_range_g;
    bool ranges_are_documented_not_measured;

    // Per-axis sign correction (x,y,z), applied to raw gyro/accel before
    // any other processing. Identity until axis_mapping_verified.
    float axis_sign[3] = {1.0f, 1.0f, 1.0f};
    bool axis_mapping_verified = false;
};

inline DeviceProfile get_device_profile(SDL_GamepadType type) {
    switch (type) {
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            return DeviceProfile{"Nintendo Switch IMU (documented default)", 2000.0f, 8.0f, true};
        default:
            // Unknown/unsupported device: do not guess. Callers must check
            // ranges_are_documented_not_measured before trusting the range.
            return DeviceProfile{"Unknown (no documented range)", 0.0f, 0.0f, false};
    }
}
