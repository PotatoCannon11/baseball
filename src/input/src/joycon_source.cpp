#include "input/joycon_source.h"

#include <cmath>

namespace input {

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kStandardGravity = 9.80665f;  // SDL_STANDARD_GRAVITY
}  // namespace

JoyconSource::JoyconSource(SDL_Gamepad* gamepad) : gamepad_(gamepad) {
    id_ = SDL_GetGamepadID(gamepad_);
    type_ = SDL_GetGamepadType(gamepad_);
    profile_ = get_device_profile(type_);

    // Enable every sensor channel this gamepad exposes. A plain Joy-Con or
    // Pro Controller exposes GYRO/ACCEL; a combined Joy-Con pair (per SDL's
    // default SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS=1) additionally/
    // instead exposes GYRO_L/ACCEL_L and GYRO_R/ACCEL_R, one pair per
    // physical half. This class does not yet have a real policy for
    // choosing/combining L vs R when both are present -- both just feed the
    // same ring buffer, "whichever event arrives" -- since the intended
    // primary use case (spec: "a single Joy-Con held sideways") only ever
    // exposes plain GYRO/ACCEL. Revisit the combined-pair case once real
    // hardware is available to test what a two-handed paired grip actually
    // needs.
    for (SDL_SensorType t : {SDL_SENSOR_GYRO, SDL_SENSOR_ACCEL, SDL_SENSOR_GYRO_L,
                              SDL_SENSOR_ACCEL_L, SDL_SENSOR_GYRO_R, SDL_SENSOR_ACCEL_R}) {
        if (SDL_GamepadHasSensor(gamepad_, t)) {
            SDL_SetGamepadSensorEnabled(gamepad_, t, true);
        }
    }
}

JoyconSource::~JoyconSource() {
    if (gamepad_) SDL_CloseGamepad(gamepad_);
}

void JoyconSource::handle_sensor_event(const SDL_GamepadSensorEvent& event) {
    const SDL_SensorType type = static_cast<SDL_SensorType>(event.sensor);
    const bool is_gyro =
        (type == SDL_SENSOR_GYRO || type == SDL_SENSOR_GYRO_L || type == SDL_SENSOR_GYRO_R);
    const bool is_accel =
        (type == SDL_SENSOR_ACCEL || type == SDL_SENSOR_ACCEL_L || type == SDL_SENSOR_ACCEL_R);
    if (!is_gyro && !is_accel) return;

    // Correlate with the most recent sample for this device so a gyro
    // event and a nearly-simultaneous accel event don't each produce a
    // half-empty ImuSample. Simplification for M2: we don't merge across
    // events, we just carry the last-known other-channel value forward.
    // Good enough while gyro is the dominant signal (bat model uses gyro
    // only); revisit if the accel channel ever needs tighter correlation.
    ImuSample sample = imu_ring_.empty() ? ImuSample{} : imu_ring_.back();
    sample.timestamp_ns = event.sensor_timestamp;

    if (is_gyro) {
        for (int axis = 0; axis < 3; ++axis) {
            sample.gyro[axis] = event.data[axis];
            if (profile_.gyro_range_dps > 0.0f) {
                const float limit = profile_.gyro_range_dps * kDegToRad;
                if (std::fabs(event.data[axis]) >= 0.98f * limit) {
                    sample.clip_flags |= static_cast<std::uint8_t>(1u << axis);
                }
            }
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) {
            sample.accel[axis] = event.data[axis];
            if (profile_.accel_range_g > 0.0f) {
                const float limit = profile_.accel_range_g * kStandardGravity;
                if (std::fabs(event.data[axis]) >= 0.98f * limit) {
                    sample.clip_flags |= static_cast<std::uint8_t>(1u << (3 + axis));
                }
            }
        }
    }

    imu_ring_.push(sample);
}

void JoyconSource::ingest(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            if (event.gsensor.which == id_) handle_sensor_event(event.gsensor);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (event.gbutton.which == id_) {
                ButtonEvent b;
                b.timestamp_ns = event.gbutton.timestamp;
                b.button = event.gbutton.button;
                b.down = event.gbutton.down;
                button_ring_.push(b);
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (event.gdevice.which == id_) connected_ = false;
            break;
        default:
            break;
    }
}

}  // namespace input
