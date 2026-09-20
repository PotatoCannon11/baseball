#include "render/camera_director.h"

#include "sim/vec3.h"

namespace render {

namespace {

Camera fixed_camera(const float eye[3], const float target[3], const CameraDirectorConfig& config) {
    Camera cam;
    cam.eye[0] = eye[0];
    cam.eye[1] = eye[1];
    cam.eye[2] = eye[2];
    cam.target[0] = target[0];
    cam.target[1] = target[1];
    cam.target[2] = target[2];
    cam.fov_radians = config.fov_radians;
    cam.near_plane = config.near_plane;
    cam.far_plane = config.far_plane;
    return cam;
}

}  // namespace

Camera compute_camera(PitchPhase phase, const sim::SimState& state, const CameraDirectorConfig& config) {
    switch (phase) {
        case PitchPhase::kWaitingForReady:
        case PitchPhase::kWindup:
            return fixed_camera(config.mound_eye, config.plate_target, config);

        case PitchPhase::kInFlight: {
            // Center-field-high is itself a fixed preset, but still
            // aim slightly toward the ball's current lateral position
            // rather than dead-center, so wide misses stay framed.
            const float target[3] = {static_cast<float>(state.ball.position.x), 1.0f,
                                      static_cast<float>(state.ball.position.z * 0.3)};
            return fixed_camera(config.center_field_eye, target, config);
        }

        case PitchPhase::kFollowBall: {
            const sim::Vec3& pos = state.ball.position;
            const sim::Vec3& vel = state.ball.velocity;
            const sim::Vec3 dir = vel.length() > 1e-6 ? vel.normalized() : sim::Vec3{0.0, 0.0, 1.0};
            const sim::Vec3 eye = pos - dir * config.follow_back_m + sim::Vec3{0.0, config.follow_up_m, 0.0};
            const sim::Vec3 target = pos + vel * config.follow_lead_seconds;

            const float eye_f[3] = {static_cast<float>(eye.x), static_cast<float>(eye.y), static_cast<float>(eye.z)};
            const float target_f[3] = {static_cast<float>(target.x), static_cast<float>(target.y),
                                        static_cast<float>(target.z)};
            return fixed_camera(eye_f, target_f, config);
        }
    }
    return fixed_camera(config.mound_eye, config.plate_target, config);  // unreachable, keeps -Wswitch happy pre-C++23
}

}  // namespace render
