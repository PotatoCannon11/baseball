// Milestone 2 acceptance test: "Terminal output of bat barrel speed while
// I swing (hard swing roughly 25-40 m/s at the barrel)." Wires together
// every milestone-2 input-layer component end to end: an ImuSource (real
// Joy-Con via SdlInputHub, or a --backend mouse/keyboard dev stand-in) ->
// gyro bias subtraction -> clip recovery -> Madgwick orientation filter ->
// the rigid-rod bat model -> a live terminal readout, with an optional
// --record to also write the resulting PlayerInput stream to disk.
//
// UNTESTED WITH REAL HARDWARE as of writing: built and run against the
// mouse/keyboard dev backends only (see README). The Joy-Con path builds
// and runs cleanly with zero devices connected, but the actual barrel
// speed number is unverified until this runs against a real swing.

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "input/motion_pipeline.h"
#include "input/recorder.h"
#include "input/sdl_input_hub.h"
#include "platform/clock.h"

namespace {

constexpr double kCalibrationSeconds = 1.0;
constexpr double kDefaultRunSeconds = 30.0;

struct Args {
    bool use_mouse = false;
    bool use_keyboard = false;
    bool debug = false;
    double seconds = kDefaultRunSeconds;
    const char* record_path = nullptr;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            if (std::strcmp(argv[i + 1], "mouse") == 0) a.use_mouse = true;
            if (std::strcmp(argv[i + 1], "keyboard") == 0) a.use_keyboard = true;
            ++i;
        } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            a.seconds = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            a.record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            a.debug = true;
        }
    }
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    // A window is required at all (SDL only delivers mouse/keyboard events
    // to a window that exists, and some platforms require a window and the
    // main thread to pump events at all) and it must be VISIBLE and
    // focusable for the mouse/keyboard dev backends specifically: a mouse
    // click has nothing to land on, and a keyboard has nothing to focus,
    // if the window is hidden. Gamepad sensor/button events don't need
    // window focus, so this is harmless for the real Joy-Con backend too.
    SDL_Window* window = SDL_CreateWindow(
        "barrel_speed -- click+drag (mouse backend) or hold SPACE (keyboard backend) here", 480, 240, 0);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    std::printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
    // A window that never presents a frame never actually becomes visible
    // on some backends -- notably Wayland, where an unmapped/never-
    // committed surface is simply not shown by the compositor at all, not
    // even as a blank rectangle. Milestone 2 has no renderer (that's
    // milestone 3's "bare 3D scene"), so this is a plain 2D SDL_Renderer
    // clear+present loop, purely so the window actually appears and stays
    // responsive -- the color is a crude live indicator (gray while
    // calibrating, brighter red the harder the swing), not real graphics.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    input::SdlInputHub hub;
    hub.set_debug_logging(args.debug);
    input::MotionPipeline pipeline;

    input::PlayerInputRecorder recorder;
    if (args.record_path && !recorder.open(args.record_path)) {
        std::fprintf(stderr, "Warning: could not open --record file '%s'; continuing without recording\n",
                     args.record_path);
    }

    if (args.use_mouse) {
        std::printf(
            "Backend: mouse drag. Click and hold the left mouse button over the window that just\n"
            "opened, and drag, to emulate a swing.\n");
    } else if (args.use_keyboard) {
        std::printf("Backend: keyboard. Hold SPACE to ramp up a synthetic swing.\n");
    } else {
        std::printf("Backend: Joy-Con/gamepad via SDL3.\n");
    }

    std::printf("Calibrating gyro bias for %.1f s -- hold the controller still...\n", kCalibrationSeconds);
    pipeline.start_bias_calibration();

    const std::uint64_t start_ns = platform::monotonic_now_ns();
    const std::uint64_t calibration_end_ns = start_ns + static_cast<std::uint64_t>(kCalibrationSeconds * 1e9);
    const std::uint64_t run_end_ns = start_ns + static_cast<std::uint64_t>(args.seconds * 1e9);

    std::uint64_t last_frame_ns = start_ns;
    std::uint32_t tick = 0;
    std::uint16_t sequence = 0;
    bool calibration_done_announced = false;

    while (platform::monotonic_now_ns() < run_end_ns) {
        const std::uint64_t now = platform::monotonic_now_ns();
        const float dt = static_cast<float>(static_cast<double>(now - last_frame_ns) / 1e9);
        last_frame_ns = now;

        hub.poll(dt);

        if (args.use_mouse) {
            pipeline.process(hub.mouse_drag_source().imu_samples());
        } else if (args.use_keyboard) {
            pipeline.process(hub.keyboard_source().imu_samples());
        } else {
            input::JoyconSource* js = hub.joycon_for_slot(input::PlayerSlot::kP1);
            if (!js) {
                // Auto-claim the first connected, unclaimed Joy-Con so this
                // tool works without a separate "press to join" step.
                const SDL_JoystickID id = hub.first_unclaimed_connected_device();
                if (id != input::BindingTable::kNoDevice && hub.claim_slot(input::PlayerSlot::kP1, id)) {
                    js = hub.joycon_for_slot(input::PlayerSlot::kP1);
                }
            }
            if (js) {
                pipeline.process(js->imu_samples());
            }
        }

        if (!calibration_done_announced && now >= calibration_end_ns) {
            pipeline.finish_bias_calibration();
            calibration_done_announced = true;
            std::printf("Calibration done. Swing now.\n");
        }

        if (calibration_done_announced) {
            const float speed = pipeline.barrel_speed_mps();
            std::printf("\rbarrel speed: %6.2f m/s   clip_flags=0x%02x   ", speed,
                        pipeline.last_clip_flags());
            std::fflush(stdout);

            if (recorder.is_open()) {
                recorder.write(pipeline.to_player_input(tick, sequence++));
            }

            // Crude visual indicator only -- brighter red the faster the
            // last-seen swing, clamped so it's readable at a glance.
            const float t = speed / 40.0f;  // 40 m/s ~ spec's "hard swing" upper end
            const Uint8 red = static_cast<Uint8>(std::fmin(1.0f, t) * 255.0f);
            SDL_SetRenderDrawColor(renderer, red, 40, 60, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);  // gray while calibrating
        }
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        ++tick;

        SDL_Delay(1);
    }

    std::printf("\n");
    if (recorder.is_open()) {
        std::printf("Recorded %zu PlayerInput frames to %s\n", recorder.frames_written(), args.record_path);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
