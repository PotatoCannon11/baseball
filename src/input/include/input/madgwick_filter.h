#pragma once

// Madgwick IMU orientation filter (gyroscope + accelerometer, no
// magnetometer -- there's no compass on a Joy-Con, and yaw has no absolute
// reference anyway, hence the spec's recenter-button requirement).
//
// Algorithm: Sebastian Madgwick, "An efficient orientation filter for
// inertial and inertial/magnetic sensor arrays", 2010. This is a direct
// translation of the widely-reproduced open-source reference
// implementation (x-io Technologies' MadgwickAHRS, originally published
// under GPL by Madgwick/x-io and later relicensed permissively; the
// algorithm itself -- gradient-descent correction of a gyro-integrated
// quaternion toward the accelerometer's gravity estimate -- is what's
// reproduced here, restructured into a small stateful class with a
// variable dt instead of a fixed sample frequency). Not re-derived from
// scratch: this closed-form gradient has been reviewed and used
// extensively in the wild, and re-deriving it risks a subtle sign error
// that would only show up as odd drift under real motion.
//
// Quaternion is stored (w, x, y, z) to match the reference algorithm's own
// q0..q3 ordering; convert at the PlayerInput quantization boundary if a
// different convention is needed there.
namespace input {

class MadgwickFilter {
public:
    // beta trades correction speed against gyro-integration smoothness;
    // 0.1 is the value the original paper and most ports settle on as a
    // reasonable default. Not yet tuned against a real Joy-Con.
    explicit MadgwickFilter(float beta = 0.1f) : beta_(beta) {}

    void reset() {
        q0_ = 1.0f;
        q1_ = q2_ = q3_ = 0.0f;
    }

    // Sets the filter's quaternion directly, (w, x, y, z). Used by tests
    // that need to start from a known non-identity state, and will also
    // back the future recenter-button feature (recentering needs to
    // rewrite yaw without needing a live gyro/accel stream to get there).
    void reset(const float initial_wxyz[4]) {
        q0_ = initial_wxyz[0];
        q1_ = initial_wxyz[1];
        q2_ = initial_wxyz[2];
        q3_ = initial_wxyz[3];
    }

    // gyro in rad/s, accel in any consistent unit (only its direction is
    // used, not magnitude) so long as [0,0,0] means "no accelerometer
    // correction this step" -- callers should pass zero when the spec's
    // "only correct tilt when accelerometer magnitude is near 1 g" gate
    // fails, e.g. during a hard swing where measured acceleration includes
    // large non-gravity components.
    void update(const float gyro[3], const float accel[3], float dt_seconds) {
        float gx = gyro[0], gy = gyro[1], gz = gyro[2];
        float ax = accel[0], ay = accel[1], az = accel[2];

        float qDot1 = 0.5f * (-q1_ * gx - q2_ * gy - q3_ * gz);
        float qDot2 = 0.5f * (q0_ * gx + q2_ * gz - q3_ * gy);
        float qDot3 = 0.5f * (q0_ * gy - q1_ * gz + q3_ * gx);
        float qDot4 = 0.5f * (q0_ * gz + q1_ * gy - q2_ * gx);

        if (!(ax == 0.0f && ay == 0.0f && az == 0.0f)) {
            float recip_norm = inv_sqrt(ax * ax + ay * ay + az * az);
            ax *= recip_norm;
            ay *= recip_norm;
            az *= recip_norm;

            const float _2q0 = 2.0f * q0_;
            const float _2q1 = 2.0f * q1_;
            const float _2q2 = 2.0f * q2_;
            const float _2q3 = 2.0f * q3_;
            const float _4q0 = 4.0f * q0_;
            const float _4q1 = 4.0f * q1_;
            const float _4q2 = 4.0f * q2_;
            const float _8q1 = 8.0f * q1_;
            const float _8q2 = 8.0f * q2_;
            const float q0q0 = q0_ * q0_;
            const float q1q1 = q1_ * q1_;
            const float q2q2 = q2_ * q2_;
            const float q3q3 = q3_ * q3_;

            float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
            float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1_ - _2q0 * ay - _4q1 +
                       _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
            float s2 = 4.0f * q0q0 * q2_ + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 +
                       _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
            float s3 = 4.0f * q1q1 * q3_ - _2q1 * ax + 4.0f * q2q2 * q3_ - _2q2 * ay;

            recip_norm = inv_sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
            s0 *= recip_norm;
            s1 *= recip_norm;
            s2 *= recip_norm;
            s3 *= recip_norm;

            qDot1 -= beta_ * s0;
            qDot2 -= beta_ * s1;
            qDot3 -= beta_ * s2;
            qDot4 -= beta_ * s3;
        }

        q0_ += qDot1 * dt_seconds;
        q1_ += qDot2 * dt_seconds;
        q2_ += qDot3 * dt_seconds;
        q3_ += qDot4 * dt_seconds;

        const float recip_norm = inv_sqrt(q0_ * q0_ + q1_ * q1_ + q2_ * q2_ + q3_ * q3_);
        q0_ *= recip_norm;
        q1_ *= recip_norm;
        q2_ *= recip_norm;
        q3_ *= recip_norm;
    }

    // (w, x, y, z)
    void quaternion(float out[4]) const {
        out[0] = q0_;
        out[1] = q1_;
        out[2] = q2_;
        out[3] = q3_;
    }

private:
    static float inv_sqrt(float x);

    float beta_;
    float q0_ = 1.0f, q1_ = 0.0f, q2_ = 0.0f, q3_ = 0.0f;
};

}  // namespace input
