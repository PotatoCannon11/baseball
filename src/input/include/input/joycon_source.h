#pragma once

#include <SDL3/SDL.h>

#include "input/device_profile.h"
#include "input/imu_source.h"

// Wraps one SDL_Gamepad believed to be a Joy-Con, Pro Controller, or
// combined Joy-Con pair. Deliberately does NOT call SDL_PollEvent itself:
// SDL's event queue is process-global, so only one place in the whole app
// may drain it (see sdl_input_hub.h, which owns that loop for every
// device and dispatches by joystick id). ingest() is how events reach
// this instance.
namespace input {

class JoyconSource : public ImuSource {
public:
    explicit JoyconSource(SDL_Gamepad* gamepad);
    ~JoyconSource() override;

    // Owns gamepad_ (closes it on destruction) -- never copy or move this.
    JoyconSource(const JoyconSource&) = delete;
    JoyconSource& operator=(const JoyconSource&) = delete;

    SDL_JoystickID id() const { return id_; }
    const DeviceProfile& profile() const { return profile_; }

    void ingest(const SDL_Event& event);
    void mark_disconnected() { connected_ = false; }

    bool is_connected() const override { return connected_; }
    const ImuRing& imu_samples() const override { return imu_ring_; }
    const ButtonRing& button_events() const override { return button_ring_; }

private:
    void handle_sensor_event(const SDL_GamepadSensorEvent& event);

    SDL_Gamepad* gamepad_ = nullptr;
    SDL_JoystickID id_ = 0;
    SDL_GamepadType type_ = SDL_GAMEPAD_TYPE_UNKNOWN;
    DeviceProfile profile_{};
    bool connected_ = true;

    ImuRing imu_ring_;
    ButtonRing button_ring_;
};

}  // namespace input
