#pragma once

#include <cstdint>

#include "common/player_input.h"
#include "input/bat_model.h"
#include "input/bias_calibration.h"
#include "input/clip_recovery.h"
#include "input/imu_types.h"
#include "input/madgwick_filter.h"

// Composes the milestone-2 pipeline stages (clip recovery -> bias
// subtraction -> accelerometer-gated orientation filter -> bat model) over
// a single ImuSource's ring buffer, tracking its own read cursor so
// repeated calls only process newly-arrived samples. This is the same
// logic tools/barrel_speed uses for its live terminal readout, pulled out
// into the input library so it's testable with synthetic data and so
// milestone 3 can reuse it without needing a copy inside a tool.
namespace input {

class MotionPipeline {
public:
    void set_arm_length(float meters) { arm_length_m_ = meters; }

    // Feeds every new sample since the last call through clip recovery,
    // bias subtraction (once calibrated), and the orientation filter.
    // Returns the number of raw samples consumed from the ring.
    std::size_t process(const ImuRing& ring);

    void start_bias_calibration() { bias_.reset(); }
    bool bias_calibrating() const { return !bias_.is_done(); }
    void finish_bias_calibration() { bias_.finish(); }

    float barrel_speed_mps() const;
    std::uint8_t last_clip_flags() const { return last_clip_flags_; }
    const float* last_gyro() const { return last_gyro_; }

    common::PlayerInput to_player_input(std::uint32_t tick, std::uint16_t sequence) const;

private:
    ClipRecoveryWindow<9> clip_window_;
    GyroBiasCalibrator bias_;
    MadgwickFilter filter_;
    std::size_t read_cursor_ = 0;
    std::uint64_t last_timestamp_ns_ = 0;
    float last_gyro_[3] = {0, 0, 0};
    std::uint8_t last_clip_flags_ = 0;
    float arm_length_m_ = kDefaultArmLengthMeters;
};

}  // namespace input
