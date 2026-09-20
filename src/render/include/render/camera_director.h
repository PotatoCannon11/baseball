#pragma once

#include "render/camera.h"
#include "sim/state.h"

// Milestone 7: "Camera framing is a pure function of sim state and
// presentation config ... It never feeds back into the sim and never
// depends on which players are human." This is the versus mode's camera
// logic: given which phase of the pitch/swing cycle the driver (versus
// join-flow loop, CPU-vs-CPU tool, etc.) says it's in, plus the live
// SimState, compute a Camera. No hidden timers, no mutable director
// object -- same (phase, state, config) in always gives the same Camera
// out, which is what makes it safe to call independently on every peer in
// a future online session without any risk of divergence.
//
// Deliberately does NOT take pitch type, grip, or aim target: the camera
// director has no way to leak that information even by accident, because
// its inputs don't contain it.
namespace render {

// Coarse phase of one pitch/swing cycle. Owned by the driver, not SimState
// -- milestone 6 already established the pattern of keeping at-bat
// orchestration state (which pitch, whether a swing was decided, etc.)
// outside SimState and in the loop that drives sim::step(); this follows
// the same split.
enum class PitchPhase : std::uint8_t {
    kWaitingForReady,  // pre-pitch: behind-the-mound framing, nothing moving yet
    kWindup,           // pitcher winding up: same framing, held through the windup
    kInFlight,         // ball released, still traveling: center-field-high framing
    kFollowBall,       // contact occurred (or the ball passed the plate): follow cam
};

struct CameraDirectorConfig {
    // Behind-the-mound preset: looking from behind the pitcher toward
    // home plate. Used for kWaitingForReady/kWindup.
    float mound_eye[3] = {0.0f, 2.4f, 20.5f};
    float plate_target[3] = {0.0f, 1.0f, 0.0f};

    // Center-field-high preset: a raised, pulled-back view of the whole
    // pitch corridor. Used for kInFlight.
    float center_field_eye[3] = {0.0f, 5.5f, 24.0f};

    // Follow-cam framing: how far behind/above the ball the eye trails,
    // and how much to look ahead of the ball along its current velocity
    // rather than straight at it (a fixed camera pointed exactly at a
    // fast-moving object whips around uncomfortably near closest
    // approach; leading it slightly avoids that without needing any
    // smoothing/lerp state, which would break the pure-function property).
    float follow_back_m = 4.0f;
    float follow_up_m = 1.6f;
    float follow_lead_seconds = 0.05f;

    float fov_radians = 0.9f;
    float near_plane = 0.05f;
    float far_plane = 200.0f;
};

Camera compute_camera(PitchPhase phase, const sim::SimState& state, const CameraDirectorConfig& config);

}  // namespace render
