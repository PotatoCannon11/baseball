#pragma once

#include <string>

#include "input/binding_table.h"

// Milestone 7 per-player calibration: "the gyro bias, virtual arm/bat
// length, gain curve, and recenter are stored per player slot (in a
// fixed struct saved in the pref path), not per device, so swapping
// controllers doesn't lose settings. Each player recenters
// independently."
namespace versus {

struct PlayerCalibration {
    float gyro_bias[3] = {0.0f, 0.0f, 0.0f};  // input::GyroBiasCalibrator::bias()
    float arm_length_m = 0.75f;               // input::kDefaultArmLengthMeters mirrored as the default
    float filter_gain = 0.1f;                 // input::MadgwickFilter's beta
    float recenter_wxyz[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // last recenter orientation offset; identity = none set
};

// Keyed by PlayerSlot -- deliberately NOT by Role (a role swap must not
// move calibration between players) and NOT by device id (a device swap
// must not reset it either). One flat file per slot under the given
// pref directory, same fixed-buffer/magic-header convention as
// input::PlayerInputRecorder.
class CalibrationStore {
public:
    // pref_dir is a directory path (e.g. from platform::get_pref_path),
    // taken as a constructor argument rather than hardcoded so tests can
    // point it at a scratch directory instead of the real user prefs
    // location.
    explicit CalibrationStore(std::string pref_dir) : pref_dir_(std::move(pref_dir)) {}

    // Returns a default-constructed PlayerCalibration if no file exists
    // yet, or it fails to parse -- a fresh slot with nothing saved is not
    // an error condition.
    PlayerCalibration load(input::PlayerSlot slot) const;

    bool save(input::PlayerSlot slot, const PlayerCalibration& calibration) const;

private:
    std::string path_for(input::PlayerSlot slot) const;
    std::string pref_dir_;
};

}  // namespace versus
