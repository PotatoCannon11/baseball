#include "input/bias_calibration.h"

#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

bool nearly(float a, float b, float eps = 1e-4f) { return (a > b ? a - b : b - a) <= eps; }

}  // namespace

int main() {
    bool ok = true;
    input::GyroBiasCalibrator calib;

    ok &= check(!calib.is_done(), "should not be done before finish()");

    // Simulate a "held still" device with a constant true bias plus tiny
    // symmetric noise: the estimated bias should converge to the true
    // constant offset, not to zero.
    const float true_bias[3] = {0.02f, -0.01f, 0.005f};
    for (int i = 0; i < 1000; ++i) {
        const float noise = (i % 2 == 0) ? 0.001f : -0.001f;  // symmetric, should average out
        float sample[3] = {true_bias[0] + noise, true_bias[1] + noise, true_bias[2] + noise};
        calib.accumulate(sample);
    }
    calib.finish();

    ok &= check(calib.is_done(), "should be done after finish()");
    ok &= check(calib.sample_count() == 1000, "sample_count should match number of accumulate() calls");
    ok &= check(nearly(calib.bias()[0], true_bias[0]), "estimated bias x should match true bias");
    ok &= check(nearly(calib.bias()[1], true_bias[1]), "estimated bias y should match true bias");
    ok &= check(nearly(calib.bias()[2], true_bias[2]), "estimated bias z should match true bias");

    float test_sample[3] = {true_bias[0] + 0.5f, true_bias[1] + 0.5f, true_bias[2] + 0.5f};
    calib.apply(test_sample);
    ok &= check(nearly(test_sample[0], 0.5f) && nearly(test_sample[1], 0.5f) && nearly(test_sample[2], 0.5f),
                "apply() should subtract the estimated bias, leaving just the true signal");

    calib.reset();
    ok &= check(!calib.is_done(), "reset() should clear the done flag");
    ok &= check(calib.sample_count() == 0, "reset() should clear the sample count");

    // Regression test: finish() with ZERO accumulated samples must still
    // complete (defaulting to zero bias), not leave is_done() stuck false
    // forever. A source that's idle until motion starts (e.g. the mouse-
    // drag dev backend) can legitimately deliver no samples during a
    // "hold still" calibration window; if finish() silently no-op'd in
    // that case, every sample fed in afterward would keep getting
    // swallowed into calibration instead of ever reaching a consumer --
    // this exactly caused the mouse backend to appear completely dead.
    calib.finish();
    ok &= check(calib.is_done(), "finish() with zero samples should still complete, not stay stuck forever");
    ok &= check(calib.bias()[0] == 0.0f && calib.bias()[1] == 0.0f && calib.bias()[2] == 0.0f,
                "finish() with zero samples should default to zero bias");

    if (ok) {
        std::printf("PASS: gyro bias calibration converges to the true constant offset\n");
        return 0;
    }
    return 1;
}
