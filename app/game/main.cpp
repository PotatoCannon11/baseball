// Milestone 3 bare 3D scene, extended by milestone 7's local-versus
// wiring. Renders the sim's interpolated state (ball, shadow, lathe bat,
// camera, debug overlay). Two dev input sources stand in for two local
// humans -- NOT real controllers (explicitly out of scope until real
// hardware is available; see docs/ORIGINAL_PROMPT.md and README.md for
// why): the mouse-drag source drives the batter role, and the keyboard
// source (SPACE) drives the pitcher role's arm sweep + throw release.
// versus::JoinFlow/step_or_pause and render::compute_camera are wired in
// so the disconnect-pause and camera-director logic actually run in the
// interactive app, not just in tests -- but there is no join/role-swap
// UI yet (roles are fixed: mouse=batter, keyboard=pitcher for the whole
// session) and no real at-bat loop (count/score/outs), so the shared HUD
// stays the dev debug overlay rather than a real versus::PublicHud. Both
// are tested independently (tests/versus_role_swap_test.cpp,
// tests/versus_hud_privacy_test.cpp) against the library directly.
//
// UNTESTED WITH REAL HARDWARE / REAL DISPLAY INTERACTION as of writing:
// built and smoke-tested headless (offscreen) in this dev environment; a
// screenshot was captured and inspected to confirm the scene actually
// looks like a field/ball/bat rather than just "doesn't crash."

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

#include "alloc/alloc_counter.h"
#include "input/motion_pipeline.h"
#include "input/sdl_input_hub.h"
#include "platform/clock.h"
#include "platform/memory.h"
#include "render/camera_director.h"
#include "render/gl.h"
#include "render/mesh.h"
#include "render/overlay.h"
#include "render/shader.h"
#include "render/shaders.h"
#include "render/shadow_map.h"
#include "render/window.h"
#include "sim/hash.h"
#include "sim/step.h"
#include "versus/join_flow.h"
#include "versus/safety.h"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kShadowMapSize = 2048;
constexpr double kCalibrationSeconds = 1.0;

// nlerp (normalized lerp), not true slerp: cheap, allocation-free, and
// plenty accurate for the small per-tick rotation deltas being
// interpolated between two adjacent sim states for rendering only (this
// never feeds back into sim state).
render::Mat4 interpolated_transform(const sim::Vec3& pos_a, const sim::Vec3& pos_b, const sim::Quat& rot_a,
                                     const sim::Quat& rot_b, double alpha) {
    const float px = static_cast<float>(pos_a.x + (pos_b.x - pos_a.x) * alpha);
    const float py = static_cast<float>(pos_a.y + (pos_b.y - pos_a.y) * alpha);
    const float pz = static_cast<float>(pos_a.z + (pos_b.z - pos_a.z) * alpha);

    double qw = rot_a.w + (rot_b.w - rot_a.w) * alpha;
    double qx = rot_a.x + (rot_b.x - rot_a.x) * alpha;
    double qy = rot_a.y + (rot_b.y - rot_a.y) * alpha;
    double qz = rot_a.z + (rot_b.z - rot_a.z) * alpha;
    const double len = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
    if (len > 0.0) {
        qw /= len;
        qx /= len;
        qy /= len;
        qz /= len;
    }

    return render::Mat4::from_quat_and_pos(static_cast<float>(qw), static_cast<float>(qx), static_cast<float>(qy),
                                            static_cast<float>(qz), px, py, pz);
}

struct Args {
    bool visible = true;
    bool screenshot = false;
    const char* screenshot_path = nullptr;
    double seconds = 0.0;  // 0 = run until window closed
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--hidden") == 0) {
            a.visible = false;
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            a.screenshot = true;
            a.screenshot_path = argv[++i];
        } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            a.seconds = std::atof(argv[++i]);
        }
    }
    return a;
}

bool write_ppm(const char* path, int width, int height, const unsigned char* rgb_bottom_up) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", width, height);
    // glReadPixels returns rows bottom-to-top; PPM wants top-to-bottom.
    for (int y = height - 1; y >= 0; --y) {
        std::fwrite(rgb_bottom_up + static_cast<std::size_t>(y) * width * 3, 1, static_cast<std::size_t>(width) * 3,
                    f);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    // Default SDL behavior is to swallow the mouse click that first gives
    // a freshly-launched window focus, rather than delivering it as a
    // real button-down event -- so the very first click on a
    // just-opened window (which is also the click that would start a
    // mouse-drag swing) does nothing. Reproduces consistently on macOS,
    // where a plain Terminal-launched app always starts unfocused;
    // whether a given Linux window manager also eats it is WM-dependent.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    render::Window window;
    if (!window.create("Baseball sandbox (milestone 3)", kWindowWidth, kWindowHeight, args.visible)) {
        SDL_Quit();
        return 1;
    }

    // --- Meshes --------------------------------------------------------
    render::Mesh ball_mesh, bat_mesh, field_mesh;
    {
        std::vector<render::Vertex> vertices;
        std::vector<std::uint32_t> indices;

        render::generate_icosphere(0.0366f, 3, &vertices, &indices);  // official MLB ball radius
        ball_mesh.upload(vertices, indices);

        // Rough wood-bat profile: knob -> handle -> taper -> barrel -> tip.
        const render::LatheProfilePoint bat_profile[] = {
            {0.00f, 0.020f}, {0.05f, 0.016f}, {0.15f, 0.014f}, {0.35f, 0.016f},
            {0.55f, 0.022f}, {0.70f, 0.032f}, {0.80f, 0.035f}, {0.86f, 0.034f}, {0.864f, 0.004f},
        };
        render::generate_lathe(bat_profile, static_cast<int>(std::size(bat_profile)), 12, &vertices, &indices);
        bat_mesh.upload(vertices, indices);

        render::generate_ground_plane(40.0f, &vertices, &indices);
        field_mesh.upload(vertices, indices);
    }

    // --- Shaders ---------------------------------------------------------
    render::Shader main_shader, shadow_shader;
    if (!main_shader.compile(render::shaders::kMainVertex, render::shaders::kMainFragment) ||
        !shadow_shader.compile(render::shaders::kShadowVertex, render::shaders::kShadowFragment)) {
        SDL_Quit();
        return 1;
    }

    render::ShadowMap shadow_map;
    if (!shadow_map.create(kShadowMapSize)) {
        SDL_Quit();
        return 1;
    }

    render::Overlay overlay;
    if (!overlay.create()) {
        SDL_Quit();
        return 1;
    }

    // Camera is a pure function of (phase, sim state, config) -- computed
    // fresh every frame below, not stored as mutable state here. phase
    // starts pre-pitch and is advanced by StepEvents in the tick loop.
    const render::CameraDirectorConfig camera_config;
    render::PitchPhase phase = render::PitchPhase::kWaitingForReady;

    // Directional light: ~30 degrees off vertical so look_at's up vector
    // never goes degenerate, high enough for a believable ball shadow.
    const float light_dir[3] = {0.5f, 1.0f, 0.3f};
    float light_dir_norm[3];
    {
        const float len =
            std::sqrt(light_dir[0] * light_dir[0] + light_dir[1] * light_dir[1] + light_dir[2] * light_dir[2]);
        light_dir_norm[0] = light_dir[0] / len;
        light_dir_norm[1] = light_dir[1] / len;
        light_dir_norm[2] = light_dir[2] / len;
    }
    const float light_distance = 25.0f;
    const float light_pos[3] = {light_dir_norm[0] * light_distance, 1.0f + light_dir_norm[1] * light_distance,
                                 light_dir_norm[2] * light_distance};
    const render::Mat4 light_view = render::Mat4::look_at(light_pos[0], light_pos[1], light_pos[2], 0.0f, 1.0f, 0.0f,
                                                            0.0f, 1.0f, 0.0f);
    const render::Mat4 light_proj = render::Mat4::orthographic(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 45.0f);
    const render::Mat4 light_space = render::Mat4::multiply(light_proj, light_view);

    // --- Sim -------------------------------------------------------------
    sim::SimConfig config;
    sim::SimState prev_state{}, curr_state{};
    sim::Rng::seed(&curr_state.rng, 1);
    // Offset from the bat's pivot_position (0, 1, 0) -- milestone 4 added
    // real bat-ball collision, so spawning the ball exactly at the pivot
    // would start it already deeply overlapping the bat.
    curr_state.ball.position = sim::Vec3{0.3, 1.0, 0.0};
    curr_state.ball.orientation = sim::Quat::identity();
    curr_state.ball.angular_velocity = sim::Vec3{0.0, 4.0, 2.0};  // gentle visible spin, milestone-3 placeholder
    curr_state.bat.orientation = sim::Quat::identity();
    prev_state = curr_state;

    // --- Input: two dev backends stand in for two local humans, no real
    // controllers. See docs/ORIGINAL_PROMPT.md / README for why. Mouse
    // drives the batter role (as before milestone 7); keyboard (SPACE)
    // now also drives the pitcher role's arm sweep + throw release,
    // exercising the second dev input path README already promised but
    // this app never actually wired up. -----------------------------------
    input::SdlInputHub hub;
    input::MotionPipeline batter_pipeline;
    input::MotionPipeline pitcher_pipeline;
    batter_pipeline.start_bias_calibration();
    pitcher_pipeline.start_bias_calibration();
    std::uint16_t batter_input_sequence = 0;
    std::uint16_t pitcher_input_sequence = 0;

    // Milestone 7 join flow: no join UI yet (see file header), so both
    // dev slots are auto-claimed once at startup rather than waiting for
    // a "press a button to join" gesture. Real disconnect/reconnect
    // (versus::observe_hub) only applies to actual SdlInputHub-tracked
    // Joy-Cons, which this dev-only build never binds to a slot -- mouse
    // and keyboard sources never disconnect, so should_pause() is always
    // false here, but step_or_pause() is still used (instead of a raw
    // sim::step() call) so this is the same code path a real controller
    // session would run.
    versus::JoinFlow join_flow;
    join_flow.mark_claimed(input::PlayerSlot::kP1);
    join_flow.mark_claimed(input::PlayerSlot::kP2);

    // Diagnostic: BASEBALL_DEBUG_INPUT=1 prints every mouse/keyboard/
    // gamepad event the hub sees to stderr, to tell "SDL isn't delivering
    // events to this window at all" (a platform/focus/permissions issue
    // upstream of this codebase) apart from "events arrive but something
    // downstream misroutes them" (a bug in here).
    if (std::getenv("BASEBALL_DEBUG_INPUT")) hub.set_debug_logging(true);

    std::printf("%s\n", versus::kSafetyReminder);

    const alloc::AllocStats startup_alloc = alloc::get_stats();
    if (std::getenv("BASEBALL_DEBUG_ALLOC")) alloc::debug_log_next_allocations(20);

    const std::uint64_t start_ns = platform::monotonic_now_ns();
    const std::uint64_t calibration_end_ns = start_ns + static_cast<std::uint64_t>(kCalibrationSeconds * 1e9);
    std::uint64_t last_frame_ns = start_ns;
    double tick_accumulator = 0.0;
    bool running = true;
    std::uint64_t last_step_ns = 0;

    while (running) {
        const std::uint64_t now = platform::monotonic_now_ns();
        const float frame_dt = static_cast<float>(static_cast<double>(now - last_frame_ns) / 1e9);
        last_frame_ns = now;

        // hub.poll() is the ONLY SDL_PollEvent call in the app (SDL's event
        // queue is global; a second poller here would steal mouse/keyboard
        // events before the hub's sources ever saw them).
        hub.poll(frame_dt);
        if (hub.quit_requested()) running = false;
        batter_pipeline.process(hub.mouse_drag_source().imu_samples());
        pitcher_pipeline.process(hub.keyboard_source().imu_samples());
        if (batter_pipeline.bias_calibrating() && now >= calibration_end_ns) {
            batter_pipeline.finish_bias_calibration();
        }
        if (pitcher_pipeline.bias_calibrating() && now >= calibration_end_ns) {
            pitcher_pipeline.finish_bias_calibration();
        }
        versus::observe_hub(&join_flow, hub);

        // Fixed-timestep sim tick loop; frame_dt drives how many ticks
        // run this frame (0, 1, or a handful if the frame ran long).
        tick_accumulator += frame_dt;
        const double dt = config.tick_dt_seconds;
        int ticks_this_frame = 0;
        while (tick_accumulator >= dt && ticks_this_frame < 8) {
            sim::SimInputs inputs{};
            inputs.pitcher = pitcher_pipeline.to_player_input(curr_state.tick + 1, pitcher_input_sequence++);
            inputs.batter = batter_pipeline.to_player_input(curr_state.tick + 1, batter_input_sequence++);

            const std::uint64_t step_start = platform::monotonic_now_ns();
            prev_state = curr_state;
            sim::StepEvents events;
            curr_state = versus::step_or_pause(curr_state, inputs, config, join_flow, &events);
            last_step_ns = platform::monotonic_now_ns() - step_start;

            // Camera phase, driven by the same events the at-bat loop
            // would use (milestone 6's cpu::at_bat / tools/cpu_vs_cpu
            // pattern) -- see render/camera_director.h's PitchPhase.
            if (events.ready_for_next_pitch) phase = render::PitchPhase::kWindup;
            if (events.ball_released_this_tick) phase = render::PitchPhase::kInFlight;
            if (events.bat_contact_occurred) phase = render::PitchPhase::kFollowBall;

            tick_accumulator -= dt;
            ++ticks_this_frame;
        }
        const double alpha = tick_accumulator / dt;
        const render::Camera camera = render::compute_camera(phase, curr_state, camera_config);

        // --- Shadow pass ---------------------------------------------------
        shadow_shader.use();
        shadow_shader.set_mat4("uLightSpace", light_space);
        shadow_map.begin_pass();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        {
            shadow_shader.set_mat4("uModel", render::Mat4::identity());
            field_mesh.draw();

            const render::Mat4 ball_model =
                interpolated_transform(prev_state.ball.position, curr_state.ball.position,
                                        prev_state.ball.orientation, curr_state.ball.orientation, alpha);
            shadow_shader.set_mat4("uModel", ball_model);
            ball_mesh.draw();

            const render::Mat4 bat_model =
                interpolated_transform(prev_state.bat.pivot_position, curr_state.bat.pivot_position,
                                        prev_state.bat.orientation, curr_state.bat.orientation, alpha);
            shadow_shader.set_mat4("uModel", bat_model);
            bat_mesh.draw();
        }

        // --- Main pass -------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int fb_w = kWindowWidth, fb_h = kWindowHeight;
        SDL_GetWindowSizeInPixels(window.sdl_window(), &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.53f, 0.75f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        main_shader.use();
        main_shader.set_mat4("uView", camera.view());
        main_shader.set_mat4("uProj", camera.projection(static_cast<float>(fb_w) / static_cast<float>(fb_h)));
        main_shader.set_mat4("uLightSpace", light_space);
        main_shader.set_vec3("uLightDir", light_dir_norm[0], light_dir_norm[1], light_dir_norm[2]);
        main_shader.set_int("uUseShadow", 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shadow_map.depth_texture());
        main_shader.set_int("uShadowMap", 0);

        main_shader.set_mat4("uModel", render::Mat4::identity());
        main_shader.set_vec3("uBaseColor", 0.30f, 0.55f, 0.20f);  // grass green
        field_mesh.draw();

        {
            const render::Mat4 ball_model =
                interpolated_transform(prev_state.ball.position, curr_state.ball.position,
                                        prev_state.ball.orientation, curr_state.ball.orientation, alpha);
            main_shader.set_mat4("uModel", ball_model);
            main_shader.set_vec3("uBaseColor", 0.92f, 0.90f, 0.85f);  // off-white ball
            ball_mesh.draw();
        }
        {
            const render::Mat4 bat_model =
                interpolated_transform(prev_state.bat.pivot_position, curr_state.bat.pivot_position,
                                        prev_state.bat.orientation, curr_state.bat.orientation, alpha);
            main_shader.set_mat4("uModel", bat_model);
            main_shader.set_vec3("uBaseColor", 0.55f, 0.35f, 0.15f);  // wood brown
            bat_mesh.draw();
        }

        // --- Debug overlay ---------------------------------------------
        {
            char line[128];
            const platform::ProcessMemory mem = platform::get_process_memory();
            const alloc::AllocStats alloc_now = alloc::get_stats();
            const double ball_speed = curr_state.ball.velocity.length();
            const double bat_omega = curr_state.bat.angular_velocity.length();

            float y = 8.0f;
            const float line_h = 10.0f;
            std::snprintf(line, sizeof(line), "tick=%u hash=0x%016llx", curr_state.tick,
                          static_cast<unsigned long long>(sim::hash_state(curr_state)));
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
            y += line_h;
            std::snprintf(line, sizeof(line), "ball speed: %.2f m/s", ball_speed);
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
            y += line_h;
            std::snprintf(line, sizeof(line), "bat angular velocity: %.2f rad/s (arm model: milestone 5)", bat_omega);
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
            y += line_h;
            std::snprintf(line, sizeof(line), "sim step: %llu ns  frame: %.2f ms",
                          static_cast<unsigned long long>(last_step_ns), frame_dt * 1000.0);
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
            y += line_h;
            std::snprintf(line, sizeof(line), "app allocations since startup: %llu",
                          static_cast<unsigned long long>(alloc_now.total_allocations - startup_alloc.total_allocations));
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
            y += line_h;
            std::snprintf(line, sizeof(line), "process memory: private=%.1f MB resident=%.1f MB",
                          mem.private_bytes / (1024.0 * 1024.0), mem.resident_bytes / (1024.0 * 1024.0));
            overlay.draw_text(8, y, line, 1.0f, 1, 1, 1, fb_w, fb_h);
        }

        const bool time_up =
            args.seconds > 0.0 && (now - start_ns) >= static_cast<std::uint64_t>(args.seconds * 1e9);
        const bool should_capture = args.screenshot && args.screenshot_path && (args.seconds <= 0.0 || time_up);
        if (should_capture) {
            std::vector<unsigned char> pixels(static_cast<std::size_t>(fb_w) * fb_h * 3);
            glReadBuffer(GL_BACK);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // glReadPixels isn't declared in render/gl.h (not needed by
            // steady-state rendering); call it via SDL's proc address
            // directly for this one-off diagnostic capture.
            static auto glReadPixels_ = reinterpret_cast<void (*)(int, int, int, int, unsigned int, unsigned int,
                                                                    void*)>(
                reinterpret_cast<void*>(SDL_GL_GetProcAddress("glReadPixels")));
            if (glReadPixels_) {
                glReadPixels_(0, 0, fb_w, fb_h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                write_ppm(args.screenshot_path, fb_w, fb_h, pixels.data());
                std::printf("Wrote screenshot to %s\n", args.screenshot_path);
            }
            running = false;
        } else if (time_up) {
            running = false;
        }

        window.swap();
    }

    overlay.destroy();
    shadow_map.destroy();
    main_shader.destroy();
    shadow_shader.destroy();
    ball_mesh.destroy();
    bat_mesh.destroy();
    field_mesh.destroy();
    window.destroy();
    SDL_Quit();
    return 0;
}
