#pragma once

#include <cmath>

// "Bat model: a rigid rod pivoting about a fixed virtual pivot. Barrel
// velocity is omega x r. No double integration of acceleration." (Double-
// integrating noisy accelerometer data drifts badly; angular velocity from
// the gyro, scaled by a fixed lever arm, is far more stable and is what
// this computes.)
namespace input {

// PROVISIONAL: a configured constant, not yet derived from a real
// calibration swing. The spec calls for "a calibration swing sets the
// virtual arm and bat length," but that needs a defined reference motion
// (e.g. a known-length arm extension, or a two-point calibration) to
// calibrate against, and designing/validating that needs a real Joy-Con
// in hand. Flagged as a follow-up once hardware is available; until then
// this constant stands in.
inline constexpr float kDefaultArmLengthMeters = 0.75f;

// Simplification: this treats the barrel as always perpendicular to the
// rotation axis (|omega x r| = |omega| * r), which holds when the bat
// sweeps in a plane roughly perpendicular to the swing's rotation axis --
// true for a normal horizontal swing, less true for an unusual one (e.g. a
// pure vertical chop). Revisit if that turns out to matter once real swing
// data is available.
inline float barrel_speed_mps(const float angular_velocity_rad_s[3], float arm_length_m) {
    const float wx = angular_velocity_rad_s[0];
    const float wy = angular_velocity_rad_s[1];
    const float wz = angular_velocity_rad_s[2];
    const float omega_mag = std::sqrt(wx * wx + wy * wy + wz * wz);
    return omega_mag * arm_length_m;
}

}  // namespace input
