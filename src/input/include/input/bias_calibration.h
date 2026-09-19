#pragma once

#include <cstddef>

// Gyro bias calibration: "measure at rest at startup and subtract." Feed
// consecutive raw gyro samples while the device is known to be
// stationary (e.g. the first second after startup, or an explicit
// "hold still" calibration step), then finish() fixes the bias and
// apply() subtracts it from every subsequent sample.
//
// No factory/hardware calibration is applied here: SDL3's gamepad sensor
// API exposes filtered/fused sensor values, not a raw calibration-register
// readout, so there is nothing to read a factory offset from through this
// path. This is a plain runtime bias estimate instead.
namespace input {

class GyroBiasCalibrator {
public:
    void accumulate(const float gyro[3]) {
        for (int i = 0; i < 3; ++i) sum_[i] += gyro[i];
        ++count_;
    }

    // Always completes calibration, even with zero samples (defaulting to
    // a zero bias in that case) rather than silently refusing to finish.
    // A source that only produces samples while actively moving (e.g. the
    // mouse-drag dev backend, which is idle until a drag starts) can
    // legitimately deliver zero samples during a "hold still" window; if
    // finish() left is_done() false forever in that case, every sample
    // fed in afterward would keep getting swallowed into calibration
    // instead of ever reaching the filter -- exactly the bug that made
    // the mouse backend appear to do nothing.
    void finish() {
        if (count_ > 0) {
            for (int i = 0; i < 3; ++i) bias_[i] = static_cast<float>(sum_[i] / static_cast<double>(count_));
        }
        done_ = true;
    }

    void reset() {
        sum_[0] = sum_[1] = sum_[2] = 0.0;
        bias_[0] = bias_[1] = bias_[2] = 0.0f;
        count_ = 0;
        done_ = false;
    }

    bool is_done() const { return done_; }
    std::size_t sample_count() const { return count_; }
    const float* bias() const { return bias_; }

    void apply(float gyro[3]) const {
        if (!done_) return;
        for (int i = 0; i < 3; ++i) gyro[i] -= bias_[i];
    }

private:
    double sum_[3] = {0.0, 0.0, 0.0};
    float bias_[3] = {0.0f, 0.0f, 0.0f};
    std::size_t count_ = 0;
    bool done_ = false;
};

}  // namespace input
