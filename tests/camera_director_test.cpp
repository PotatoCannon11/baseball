#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "render/camera_director.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

bool cameras_equal(const render::Camera& a, const render::Camera& b) {
    for (int i = 0; i < 3; ++i) {
        if (a.eye[i] != b.eye[i] || a.target[i] != b.target[i] || a.up[i] != b.up[i]) return false;
    }
    return a.fov_radians == b.fov_radians && a.near_plane == b.near_plane && a.far_plane == b.far_plane;
}

}  // namespace

// "Camera framing is a pure function of sim state and presentation
// config ... It never feeds back into the sim and never depends on which
// players are human." Checked here: repeated calls with the same inputs
// give bit-identical output (no hidden mutable director state), and the
// function's signature itself cannot leak pitch-type/grip/aim data since
// it never receives it.
int main() {
    bool ok = true;

    sim::SimState state{};
    state.ball.position = sim::Vec3{1.0, 1.5, 10.0};
    state.ball.velocity = sim::Vec3{-2.0, 0.5, -35.0};
    const render::CameraDirectorConfig config;

    // --- Purity: same phase/state/config -> identical camera, every time ---
    for (render::PitchPhase phase : {render::PitchPhase::kWaitingForReady, render::PitchPhase::kWindup,
                                      render::PitchPhase::kInFlight, render::PitchPhase::kFollowBall}) {
        const render::Camera a = render::compute_camera(phase, state, config);
        const render::Camera b = render::compute_camera(phase, state, config);
        ok &= check(cameras_equal(a, b), "compute_camera must be a pure function (identical inputs -> identical output)");
    }

    // --- Pre-pitch phases use the fixed mound preset, independent of ball motion ---
    {
        sim::SimState other = state;
        other.ball.position = sim::Vec3{50.0, 50.0, 50.0};  // wildly different, should not matter pre-release
        const render::Camera a = render::compute_camera(render::PitchPhase::kWaitingForReady, state, config);
        const render::Camera b = render::compute_camera(render::PitchPhase::kWaitingForReady, other, config);
        ok &= check(cameras_equal(a, b), "pre-pitch framing should be a fixed preset, not depend on ball state");
        ok &= check(a.eye[0] == config.mound_eye[0] && a.eye[1] == config.mound_eye[1] && a.eye[2] == config.mound_eye[2],
                    "kWaitingForReady should use the configured mound-eye preset exactly");
    }

    // --- Follow cam tracks the ball ---
    {
        const render::Camera cam = render::compute_camera(render::PitchPhase::kFollowBall, state, config);
        const double dx = cam.eye[0] - static_cast<float>(state.ball.position.x);
        const double dy = cam.eye[1] - static_cast<float>(state.ball.position.y);
        const double dz = cam.eye[2] - static_cast<float>(state.ball.position.z);
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        ok &= check(dist > 0.5 && dist < 20.0, "follow cam's eye should stay a plausible fixed distance from the ball");

        sim::SimState moved = state;
        moved.ball.position = state.ball.position + sim::Vec3{5.0, 0.0, 0.0};
        const render::Camera cam2 = render::compute_camera(render::PitchPhase::kFollowBall, moved, config);
        ok &= check(cam.eye[0] != cam2.eye[0], "follow cam should move when the ball it's tracking moves");
    }

    if (ok) {
        std::printf("PASS: camera director is a pure, deterministic function of phase + sim state\n");
        return 0;
    }
    return 1;
}
