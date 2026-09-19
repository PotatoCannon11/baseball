// Unit-tests MouseKeyboardSource by hand-constructing SDL_Event structs
// and calling ingest() directly -- no real window, mouse, or keyboard
// needed, since mouse/keyboard motion doesn't require the virtual-device
// trick joycon_source_sdl_integration_test.cpp needs for gamepad sensors.

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "input/mouse_keyboard_source.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

SDL_Event make_mouse_button_event(SDL_EventType type, Uint8 button, bool down, Uint64 timestamp_ns) {
    SDL_Event e{};
    e.type = type;
    e.button.type = type;
    e.button.timestamp = timestamp_ns;
    e.button.button = button;
    e.button.down = down;
    return e;
}

SDL_Event make_mouse_motion_event(float xrel, float yrel, Uint64 timestamp_ns) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.timestamp = timestamp_ns;
    e.motion.xrel = xrel;
    e.motion.yrel = yrel;
    return e;
}

SDL_Event make_key_event(SDL_EventType type, SDL_Keycode key, bool down, bool repeat, Uint64 timestamp_ns) {
    SDL_Event e{};
    e.type = type;
    e.key.type = type;
    e.key.timestamp = timestamp_ns;
    e.key.key = key;
    e.key.down = down;
    e.key.repeat = repeat;
    return e;
}

}  // namespace

int main() {
    // pump_with_dt() calls SDL_GetTicksNS(), which needs SDL initialized at
    // least minimally on some platforms even though it isn't tied to a
    // specific subsystem.
    SDL_Init(0);

    bool ok = true;

    // --- Mouse-drag mode ---
    {
        input::MouseKeyboardSource source(input::MouseKeyboardSource::Mode::kMouseDrag);

        source.ingest(make_mouse_button_event(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, true, 1'000'000'000ull));
        ok &= check(source.button_events().size() == 1, "mouse button down should push exactly one button event");
        ok &= check(source.button_events().back().down, "the pushed button event should be 'down'");

        // 10 ms later, dragged (100, 50) px -> velocity (10000, 5000) px/s.
        source.ingest(make_mouse_motion_event(100.0f, 50.0f, 1'010'000'000ull));
        ok &= check(source.imu_samples().size() == 1, "drag motion while held should push exactly one synthetic IMU sample");
        if (source.imu_samples().size() == 1) {
            const input::ImuSample& s = source.imu_samples().back();
            // gain = 0.02: axis0 (pitch-like) from yrel*gain = 5000*0.02=100,
            // axis1 (yaw-like) from xrel*gain = 10000*0.02=200.
            ok &= check(std::fabs(s.gyro[0] - 100.0f) < 1.0f, "vertical drag should map to gyro axis 0 at the expected gain");
            ok &= check(std::fabs(s.gyro[1] - 200.0f) < 1.0f, "horizontal drag should map to gyro axis 1 at the expected gain");
        }

        source.ingest(make_mouse_button_event(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT, false, 1'020'000'000ull));
        ok &= check(source.button_events().size() == 2, "mouse button up should push a second button event");
        ok &= check(!source.button_events().back().down, "the second button event should be 'up'");
        ok &= check(source.imu_samples().size() == 2, "releasing the drag should push a zero-reset IMU sample");
        if (source.imu_samples().size() == 2) {
            const input::ImuSample& s = source.imu_samples().back();
            ok &= check(s.gyro[0] == 0.0f && s.gyro[1] == 0.0f && s.gyro[2] == 0.0f,
                        "releasing the drag should reset the synthetic angular velocity to zero");
        }

        // Motion while NOT dragging should be ignored.
        source.ingest(make_mouse_motion_event(500.0f, 500.0f, 1'030'000'000ull));
        ok &= check(source.imu_samples().size() == 2, "motion while not dragging should not push a new sample");
    }

    // --- Keyboard mode ---
    {
        input::MouseKeyboardSource source(input::MouseKeyboardSource::Mode::kKeyboard);

        source.ingest(make_key_event(SDL_EVENT_KEY_DOWN, SDLK_SPACE, true, false, 2'000'000'000ull));
        ok &= check(source.button_events().size() == 1, "SPACE down should push a button event");
        ok &= check(source.button_events().back().down, "the pushed button event should be 'down'");

        // A repeated key-down (OS key-repeat) should NOT push a second event.
        source.ingest(make_key_event(SDL_EVENT_KEY_DOWN, SDLK_SPACE, true, true, 2'010'000'000ull));
        ok &= check(source.button_events().size() == 1, "a repeat key-down event should be ignored");

        // Ramp should climb toward the target while held.
        for (int i = 0; i < 50; ++i) source.pump_with_dt(1.0f / 240.0f);
        ok &= check(source.imu_samples().size() >= 1, "pump_with_dt should push synthetic IMU samples while a key is held");
        const float omega_after_hold = source.imu_samples().back().gyro[0];
        ok &= check(omega_after_hold > 0.0f, "angular velocity should have ramped up above zero while SPACE is held");

        source.ingest(make_key_event(SDL_EVENT_KEY_UP, SDLK_SPACE, false, false, 2'500'000'000ull));
        ok &= check(source.button_events().size() == 2, "SPACE up should push a second button event");
        ok &= check(!source.button_events().back().down, "the second button event should be 'up'");

        for (int i = 0; i < 50; ++i) source.pump_with_dt(1.0f / 240.0f);
        const float omega_after_release = source.imu_samples().back().gyro[0];
        ok &= check(omega_after_release < omega_after_hold,
                    "angular velocity should ramp back down after releasing SPACE");
        std::printf("keyboard ramp: while held=%.2f rad/s, after release=%.2f rad/s\n", omega_after_hold,
                    omega_after_release);
    }

    SDL_Quit();

    if (ok) {
        std::printf("PASS: mouse-drag and keyboard dev sources convert synthetic input into ImuSample/ButtonEvent as expected\n");
        return 0;
    }
    return 1;
}
