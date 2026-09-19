#pragma once

#include <cstddef>

#include "input/imu_types.h"

// "Detect clipping using the device profile, flag frames, and recover the
// true peak by fitting a smooth bell-shaped curve to the unclipped samples
// on both sides." A quadratic (parabola) is the simplest curve with that
// shape and a closed-form least-squares fit; a true "bell curve" (Gaussian)
// fit would need nonlinear least squares for marginal benefit over the
// handful of samples in a clip event, so this deliberately uses the
// simpler quadratic and calls that out rather than silently overclaiming
// "bell-shaped".
//
// Steady-state latency is WindowSize/2 samples once the window has filled:
// a (possibly corrected) sample was pushed that many pushes ago, not just
// now. Below full, latency ramps up from 0 instead of withholding output
// entirely until WindowSize samples have EVER been pushed -- important for
// a source that produces samples in short bursts (e.g. the mouse-drag dev
// backend, idle except while actively dragging): "wait for 9 cumulative
// samples across the object's entire lifetime" could mean a short drag
// never produces any output at all, since the window was still filling
// from scratch (no continuous idle stream feeds it the rest of the way,
// unlike a real gyro). Samples returned before the window is full simply
// aren't clip-corrected (not enough neighbors to fit a curve yet); they're
// still real, useful data.
namespace input {

template <std::size_t WindowSize = 9>
class ClipRecoveryWindow {
    static_assert(WindowSize >= 5 && WindowSize % 2 == 1,
                  "need an odd window with room on both sides of the center");

public:
    static constexpr std::size_t kCenter = WindowSize / 2;
    static constexpr std::size_t kLatencySamples = kCenter;

    // Always produces an output for every push (returns true unless never
    // called before -- in practice this never returns false since the
    // first push always has something to report).
    bool push(const ImuSample& raw, ImuSample* out) {
        for (std::size_t i = 0; i + 1 < WindowSize; ++i) buffer_[i] = buffer_[i + 1];
        buffer_[WindowSize - 1] = raw;
        if (filled_ < WindowSize) ++filled_;

        if (filled_ < WindowSize) {
            // Window still filling: not enough context to center a fit on
            // anything yet, so pass the oldest buffered (valid) sample
            // through unmodified rather than withholding output.
            *out = buffer_[WindowSize - filled_];
            return true;
        }

        ImuSample result = buffer_[kCenter];
        for (int axis = 0; axis < 6; ++axis) {
            if (!(result.clip_flags & (1u << axis))) continue;
            float fitted;
            if (fit_quadratic_at_center(axis, &fitted)) {
                axis_value(result, axis) = fitted;
            }
        }
        *out = result;
        return true;
    }

private:
    static float& axis_value(ImuSample& s, int axis) {
        return axis < 3 ? s.gyro[axis] : s.accel[axis - 3];
    }
    static float axis_value(const ImuSample& s, int axis) {
        return axis < 3 ? s.gyro[axis] : s.accel[axis - 3];
    }

    // Least-squares quadratic fit y = a*x^2 + b*x + c over the unclipped
    // points in the window (x = position within the window, 0..WindowSize-1),
    // evaluated at x = kCenter. Clipped points are excluded from the fit
    // itself so the saturation being corrected doesn't corrupt the curve
    // used to correct it.
    bool fit_quadratic_at_center(int axis, float* out_value) const {
        double sx = 0, sx2 = 0, sx3 = 0, sx4 = 0, sy = 0, sxy = 0, sx2y = 0;
        int n = 0;
        for (std::size_t i = 0; i < WindowSize; ++i) {
            if (buffer_[i].clip_flags & (1u << axis)) continue;
            const double x = static_cast<double>(i);
            const double y = axis_value(buffer_[i], axis);
            sx += x;
            sx2 += x * x;
            sx3 += x * x * x;
            sx4 += x * x * x * x;
            sy += y;
            sxy += x * y;
            sx2y += x * x * y;
            ++n;
        }
        if (n < 3) return false;  // not enough unclipped points to fit a parabola

        const double A[3][3] = {
            {sx4, sx3, sx2},
            {sx3, sx2, sx},
            {sx2, sx, static_cast<double>(n)},
        };
        const double B[3] = {sx2y, sxy, sy};
        double coeffs[3];
        if (!solve_3x3(A, B, coeffs)) return false;

        const double x = static_cast<double>(kCenter);
        *out_value = static_cast<float>(coeffs[0] * x * x + coeffs[1] * x + coeffs[2]);
        return true;
    }

    // Small fixed 3x3 solve via Cramer's rule -- not worth a linear algebra
    // dependency for a system this size and this rare (only runs on
    // clipped samples).
    static bool solve_3x3(const double A[3][3], const double B[3], double out[3]) {
        auto det3 = [](double m00, double m01, double m02, double m10, double m11, double m12,
                        double m20, double m21, double m22) {
            return m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) +
                   m02 * (m10 * m21 - m11 * m20);
        };
        const double det =
            det3(A[0][0], A[0][1], A[0][2], A[1][0], A[1][1], A[1][2], A[2][0], A[2][1], A[2][2]);
        if (det == 0.0) return false;

        out[0] = det3(B[0], A[0][1], A[0][2], B[1], A[1][1], A[1][2], B[2], A[2][1], A[2][2]) / det;
        out[1] = det3(A[0][0], B[0], A[0][2], A[1][0], B[1], A[1][2], A[2][0], B[2], A[2][2]) / det;
        out[2] = det3(A[0][0], A[0][1], B[0], A[1][0], A[1][1], B[1], A[2][0], A[2][1], B[2]) / det;
        return true;
    }

    ImuSample buffer_[WindowSize] = {};
    std::size_t filled_ = 0;
};

}  // namespace input
