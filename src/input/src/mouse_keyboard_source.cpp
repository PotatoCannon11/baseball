#include "input/mouse_keyboard_source.h"

namespace input {

namespace {
constexpr std::uint16_t kSwingButtonId = 1;
}  // namespace

void MouseKeyboardSource::push_gyro_sample(std::uint64_t timestamp_ns, float wx, float wy,
                                            float wz) {
    ImuSample s;
    s.timestamp_ns = timestamp_ns;
    s.gyro[0] = wx;
    s.gyro[1] = wy;
    s.gyro[2] = wz;
    // Synthetic source: accel stays zero and unclipped (no real gravity
    // vector to report), which the Madgwick filter interprets as "skip
    // accelerometer correction this step" -- fine for a dev stand-in.
    imu_ring_.push(s);
}

void MouseKeyboardSource::ingest(const SDL_Event& event) {
    if (mode_ == Mode::kMouseDrag) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging_ = true;
                    last_motion_ns_ = event.button.timestamp;
                    ButtonEvent b;
                    b.timestamp_ns = event.button.timestamp;
                    b.button = kSwingButtonId;
                    b.down = true;
                    button_ring_.push(b);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    dragging_ = false;
                    drag_omega_[0] = drag_omega_[1] = drag_omega_[2] = 0.0f;
                    push_gyro_sample(event.button.timestamp, 0, 0, 0);
                    ButtonEvent b;
                    b.timestamp_ns = event.button.timestamp;
                    b.button = kSwingButtonId;
                    b.down = false;
                    button_ring_.push(b);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (dragging_) {
                    const std::uint64_t now = event.motion.timestamp;
                    const double dt =
                        (now > last_motion_ns_) ? static_cast<double>(now - last_motion_ns_) / 1e9 : 0.0;
                    last_motion_ns_ = now;
                    if (dt > 0.0) {
                        // Arbitrary but consistent mapping: horizontal drag
                        // -> axis 1 (yaw-like), vertical drag -> axis 0
                        // (pitch-like). Not meant to be physically
                        // meaningful, just a consistent synthetic signal.
                        const float vx = static_cast<float>(event.motion.xrel / dt);
                        const float vy = static_cast<float>(event.motion.yrel / dt);
                        drag_omega_[0] = vy * kDragGainRadPerSecPerPxPerSec;
                        drag_omega_[1] = vx * kDragGainRadPerSecPerPxPerSec;
                        push_gyro_sample(now, drag_omega_[0], drag_omega_[1], drag_omega_[2]);
                    }
                }
                break;
            default:
                break;
        }
    } else {  // Mode::kKeyboard
        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            if (event.key.key == SDLK_SPACE && !event.key.repeat) {
                key_swing_held_ = (event.type == SDL_EVENT_KEY_DOWN);
                ButtonEvent b;
                b.timestamp_ns = event.key.timestamp;
                b.button = kSwingButtonId;
                b.down = key_swing_held_;
                button_ring_.push(b);
            }
        }
    }
}

void MouseKeyboardSource::pump_with_dt(float dt_seconds) {
    const std::uint64_t now = SDL_GetTicksNS();

    if (mode_ == Mode::kMouseDrag) {
        if (dragging_ && (now - last_motion_ns_) > kDragIdleTimeoutNs) {
            // Held still mid-drag: decay toward zero rather than reporting
            // a stale nonzero angular velocity forever.
            drag_omega_[0] = drag_omega_[1] = drag_omega_[2] = 0.0f;
            push_gyro_sample(now, 0, 0, 0);
        }
    } else {
        const float target = key_swing_held_ ? kKeyboardTargetOmega : 0.0f;
        const float max_step = kKeyboardRampPerSecond * dt_seconds;
        if (keyboard_omega_ < target) {
            keyboard_omega_ = (keyboard_omega_ + max_step < target) ? keyboard_omega_ + max_step : target;
        } else if (keyboard_omega_ > target) {
            keyboard_omega_ = (keyboard_omega_ - max_step > target) ? keyboard_omega_ - max_step : target;
        }
        push_gyro_sample(now, keyboard_omega_, 0, 0);
    }
}

}  // namespace input
