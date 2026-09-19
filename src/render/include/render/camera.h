#pragma once

#include "render/mat4.h"

// A pure function of render config -- per spec, camera framing "never
// feeds back into the sim and never depends on which players are human."
// Milestone 3 scope: one fixed, simple framing. The pitch/swing vs.
// follow-cam cuts described in the spec are presentation polish that
// depends on at-bat state that doesn't exist until milestone 6; this is
// intentionally just "a camera," not the final behavior.
namespace render {

struct Camera {
    float eye[3] = {0.0f, 2.2f, 7.0f};
    float target[3] = {0.0f, 1.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    float fov_radians = 0.9f;
    float near_plane = 0.05f;
    float far_plane = 200.0f;

    Mat4 view() const { return Mat4::look_at(eye[0], eye[1], eye[2], target[0], target[1], target[2], up[0], up[1], up[2]); }
    Mat4 projection(float aspect) const { return Mat4::perspective(fov_radians, aspect, near_plane, far_plane); }
};

}  // namespace render
