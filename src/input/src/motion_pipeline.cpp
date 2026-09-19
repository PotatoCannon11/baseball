#include "input/motion_pipeline.h"

#include <cmath>

namespace input {

namespace {
constexpr float kStandardGravity = 9.80665f;  // SDL_STANDARD_GRAVITY
}  // namespace

std::size_t MotionPipeline::process(const ImuRing& ring) {
    const std::size_t total = ring.total_pushed();
    std::size_t processed = 0;
    while (read_cursor_ < total) {
        const std::size_t age_from_newest = total - 1 - read_cursor_;
        if (age_from_newest >= ring.size()) {
            // Overwritten before we could read it: skip ahead. Only
            // happens if this consumer falls far behind the ring's
            // capacity, which would itself be worth flagging separately.
            read_cursor_ = total - ring.size();
            continue;
        }
        const ImuSample& raw = ring[ring.size() - 1 - age_from_newest];

        ImuSample recovered;
        const bool have_output = clip_window_.push(raw, &recovered);
        ++read_cursor_;
        ++processed;
        if (!have_output) continue;

        if (!bias_.is_done()) {
            bias_.accumulate(recovered.gyro);
            continue;  // still calibrating; don't feed the filter yet
        }
        bias_.apply(recovered.gyro);

        // Per spec: "Correct tilt from the accelerometer only when its
        // magnitude is near 1 g." A synthetic dev source reports accel=0,
        // which the filter already treats as "skip"; a real device is
        // gated here instead.
        float accel_for_filter[3] = {0, 0, 0};
        const float accel_mag = std::sqrt(recovered.accel[0] * recovered.accel[0] +
                                           recovered.accel[1] * recovered.accel[1] +
                                           recovered.accel[2] * recovered.accel[2]);
        if (accel_mag > 0.0f && std::fabs(accel_mag - kStandardGravity) < 0.1f * kStandardGravity) {
            accel_for_filter[0] = recovered.accel[0];
            accel_for_filter[1] = recovered.accel[1];
            accel_for_filter[2] = recovered.accel[2];
        }

        const double dt = last_timestamp_ns_ == 0
                               ? (1.0 / 240.0)
                               : static_cast<double>(recovered.timestamp_ns - last_timestamp_ns_) / 1e9;
        last_timestamp_ns_ = recovered.timestamp_ns;
        if (dt > 0.0 && dt < 1.0) {
            filter_.update(recovered.gyro, accel_for_filter, static_cast<float>(dt));
        }

        last_gyro_[0] = recovered.gyro[0];
        last_gyro_[1] = recovered.gyro[1];
        last_gyro_[2] = recovered.gyro[2];
        last_clip_flags_ = recovered.clip_flags;
    }
    return processed;
}

float MotionPipeline::barrel_speed_mps() const { return input::barrel_speed_mps(last_gyro_, arm_length_m_); }

common::PlayerInput MotionPipeline::to_player_input(std::uint32_t tick, std::uint16_t sequence) const {
    common::PlayerInput pi;
    pi.tick = tick;
    pi.sequence = sequence;
    float q[4];
    filter_.quaternion(q);  // (w, x, y, z)
    pi.orientation[0] = static_cast<std::int16_t>(q[1] * common::PlayerInput::kQuatScale);
    pi.orientation[1] = static_cast<std::int16_t>(q[2] * common::PlayerInput::kQuatScale);
    pi.orientation[2] = static_cast<std::int16_t>(q[3] * common::PlayerInput::kQuatScale);
    pi.orientation[3] = static_cast<std::int16_t>(q[0] * common::PlayerInput::kQuatScale);
    for (int i = 0; i < 3; ++i) {
        pi.angular_velocity[i] = static_cast<std::int16_t>(last_gyro_[i] * common::PlayerInput::kAngVelScale);
    }
    pi.clip_flags = last_clip_flags_;
    return pi;
}

}  // namespace input
