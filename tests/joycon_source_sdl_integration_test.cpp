// This is the one test in the suite that exercises the REAL SDL event
// path end to end, without needing physical hardware: SDL3 supports
// "virtual joysticks" (SDL_AttachVirtualJoystick), fake devices that exist
// entirely in software but flow through SDL's actual internal gamepad and
// sensor processing exactly like real hardware would. We attach one
// configured as a generic gamepad with a gyro+accel sensor pair, inject
// synthetic sensor readings via SDL_SendJoystickVirtualSensorData(), pump
// the real SDL event queue, and confirm JoyconSource -- unmodified, the
// same class a real Joy-Con uses -- receives them correctly.
//
// This is NOT a substitute for testing against a real Joy-Con (it can't
// tell us anything about real sample rates, batching, clip behavior, or
// axis mapping -- those need real hardware, see docs/ORIGINAL_PROMPT.md).
// What it does prove: the SDL event dispatch, JoyconSource::ingest(), and
// the ring buffer storage are wired together correctly, independent of
// whatever a real device's driver quirks turn out to be.

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>

#include "input/joycon_source.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_VirtualJoystickSensorDesc sensors[] = {
        {SDL_SENSOR_ACCEL, 0.0f},
        {SDL_SENSOR_GYRO, 0.0f},
    };
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.nsensors = SDL_arraysize(sensors);
    desc.sensors = sensors;

    const SDL_JoystickID virtual_id = SDL_AttachVirtualJoystick(&desc);
    if (!check(virtual_id != 0, "SDL_AttachVirtualJoystick should succeed")) {
        std::fprintf(stderr, "  SDL_GetError(): %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(virtual_id);
    if (!check(gamepad != nullptr, "the virtual device should be openable as a gamepad")) {
        std::fprintf(stderr, "  SDL_GetError(): %s\n", SDL_GetError());
        SDL_DetachVirtualJoystick(virtual_id);
        SDL_Quit();
        return 1;
    }

    bool ok = true;

    // Scoped so JoyconSource's destructor (which calls SDL_CloseGamepad)
    // runs before SDL_DetachVirtualJoystick/SDL_Quit below, not after --
    // calling SDL functions on a handle after SDL_Quit() is unsafe.
    {
        // JoyconSource takes ownership and enables whatever sensors the
        // device reports having -- exactly what happens for a real Joy-Con.
        input::JoyconSource source(gamepad);
        ok &= check(source.id() == virtual_id, "JoyconSource should report the same joystick id SDL assigned");

        SDL_Joystick* underlying_joystick = SDL_GetGamepadJoystick(gamepad);
        ok &= check(underlying_joystick != nullptr,
                    "should be able to get the underlying SDL_Joystick to inject sensor data into");

        // Inject one gyro reading and pump it through the real SDL event queue.
        const float gyro_reading[3] = {1.5f, -0.5f, 0.25f};
        const std::uint64_t injected_timestamp = 123456789ull;
        const bool sent = SDL_SendJoystickVirtualSensorData(underlying_joystick, SDL_SENSOR_GYRO,
                                                              injected_timestamp, gyro_reading, 3);
        ok &= check(sent, "SDL_SendJoystickVirtualSensorData should succeed for an enabled sensor");

        bool saw_sensor_event = false;
        SDL_Event event;
        // A handful of pumps: SDL processes virtual device updates on its
        // own schedule, not necessarily immediately available on the very
        // next poll.
        for (int attempt = 0; attempt < 50 && !saw_sensor_event; ++attempt) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_GAMEPAD_SENSOR_UPDATE && event.gsensor.which == virtual_id) {
                    source.ingest(event);
                    saw_sensor_event = true;
                }
            }
            if (!saw_sensor_event) SDL_Delay(5);
        }

        ok &= check(saw_sensor_event,
                    "should have received a real SDL_EVENT_GAMEPAD_SENSOR_UPDATE for the injected gyro data");

        if (saw_sensor_event) {
            ok &= check(source.imu_samples().size() >= 1, "JoyconSource should have stored at least one sample");
            if (source.imu_samples().size() >= 1) {
                const input::ImuSample& sample = source.imu_samples().back();
                constexpr float kEps = 1e-4f;
                ok &= check(std::fabs(sample.gyro[0] - gyro_reading[0]) < kEps &&
                                std::fabs(sample.gyro[1] - gyro_reading[1]) < kEps &&
                                std::fabs(sample.gyro[2] - gyro_reading[2]) < kEps,
                            "the value that reached JoyconSource's ring buffer should match what was injected");
                std::printf("received gyro sample: (%.4f, %.4f, %.4f), timestamp=%llu\n", sample.gyro[0],
                            sample.gyro[1], sample.gyro[2], static_cast<unsigned long long>(sample.timestamp_ns));
            }
        }
    }

    SDL_DetachVirtualJoystick(virtual_id);
    SDL_Quit();

    if (ok) {
        std::printf("PASS: a virtual (software-only) gamepad's injected sensor data flows through the real SDL event queue into JoyconSource correctly\n");
        return 0;
    }
    return 1;
}
