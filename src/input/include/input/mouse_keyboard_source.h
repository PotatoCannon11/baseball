#pragma once

#include <SDL3/SDL.h>

#include "input/imu_source.h"

// Dev-only input source needing no Joy-Con: mouse-drag emulates a swing
// (drag velocity -> synthetic angular velocity), and a fixed keyboard
// mapping gives a second local dev "player" without a second controller.
// Produces the exact same ImuSample/ButtonEvent types a real device would,
// so it exercises the same downstream path (bias/filter/clip
// recovery/PlayerInput quantization) as real hardware.
//
// Like JoyconSource, does not call SDL_PollEvent itself; fed by
// sdl_input_hub.h's single central pump via ingest().
namespace input {

class MouseKeyboardSource : public ImuSource {
public:
    // Selects the key mapping so a mouse-drag instance and a keyboard
    // instance can both run at once as two independent dev "controllers"
    // without fighting over the same input.
    enum class Mode { kMouseDrag, kKeyboard };
    explicit MouseKeyboardSource(Mode mode) : mode_(mode) {}

    void ingest(const SDL_Event& event);

    // Neither mapping is purely event-driven (a held key/an idle drag both
    // need to decay toward zero without new events), so call this once per
    // frame with the frame's dt.
    void pump_with_dt(float dt_seconds);
    void pump() override { pump_with_dt(1.0f / 240.0f); }

    bool is_connected() const override { return true; }  // dev source, always "connected"
    const ImuRing& imu_samples() const override { return imu_ring_; }
    const ButtonRing& button_events() const override { return button_ring_; }

private:
    void push_gyro_sample(std::uint64_t timestamp_ns, float wx, float wy, float wz);

    Mode mode_;

    // Mouse-drag state. Not a measured gain -- tuned by feel; there's no
    // "correct" value since this is a synthetic stand-in for a real gyro.
    static constexpr float kDragGainRadPerSecPerPxPerSec = 0.02f;
    static constexpr std::uint64_t kDragIdleTimeoutNs = 50'000'000;  // 50 ms
    bool dragging_ = false;
    std::uint64_t last_motion_ns_ = 0;
    float drag_omega_[3] = {0, 0, 0};

    // Keyboard state: held key ramps a synthetic angular velocity toward a
    // fixed target on a fixed axis; released key ramps back to zero. Not a
    // realistic swing profile, just enough to exercise the pipeline.
    static constexpr float kKeyboardTargetOmega = 20.0f;  // rad/s, arbitrary dev placeholder
    static constexpr float kKeyboardRampPerSecond = 60.0f;
    bool key_swing_held_ = false;
    float keyboard_omega_ = 0.0f;

    ImuRing imu_ring_;
    ButtonRing button_ring_;
};

}  // namespace input
