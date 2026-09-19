#pragma once

#include "sim/vec3.h"

// "Spin: face-button grip preset selects a 3x3 matrix from controller
// angular velocity to ball spin, plus default axis, efficiency, and speed
// penalty." Four presets, selected by common::InputButton::kGripPreset1-4,
// flavored after real pitch grips -- the specific numbers are a game-feel
// starting point, not measured data, and are the obvious first thing to
// expose in milestone 9's property editor.
namespace sim {

struct GripPreset {
    // Row-major 3x3: spin = matrix * controller_angular_velocity. Identity
    // means "spin exactly mirrors hand angular velocity"; grips that
    // should suppress or redirect some axis (e.g. a curveball wanting
    // mostly topspin regardless of incidental hand wobble) use a
    // non-identity matrix.
    double matrix[9] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    };
    // Direction of the finger-impulse spin contribution (see
    // sim/throw.cpp's throw_ball) -- the direction that grip's finger
    // action characteristically snaps in, independent of the raw
    // controller-angular-velocity-derived spin above.
    Vec3 default_axis{0.0, 1.0, 0.0};
    double efficiency = 1.0;      // fraction of arm energy that becomes spin rather than speed
    double speed_penalty = 0.0;   // fraction subtracted from release speed for this grip

    Vec3 apply(const Vec3& controller_angular_velocity) const {
        return Vec3{
            matrix[0] * controller_angular_velocity.x + matrix[1] * controller_angular_velocity.y +
                matrix[2] * controller_angular_velocity.z,
            matrix[3] * controller_angular_velocity.x + matrix[4] * controller_angular_velocity.y +
                matrix[5] * controller_angular_velocity.z,
            matrix[6] * controller_angular_velocity.x + matrix[7] * controller_angular_velocity.y +
                matrix[8] * controller_angular_velocity.z,
        };
    }
};

inline constexpr int kGripPresetCount = 4;

struct GripTable {
    // NOTE on the x-row sign: in this codebase's throwing convention
    // (release_state_from_arm, sim/throw.cpp), the arm's angular velocity
    // about world x is what the omega x lever relation ties to the
    // pitch's own release angle -- for a pitch thrown generally in -z,
    // that same raw x-component of hand angular velocity produces a
    // DOWNWARD (topspin-like) Magnus force if passed straight through
    // (verified: domega/dt's omega x v term works out to
    // -omega.x * v.z in the dominant y-component, and v.z < 0, so a
    // physically-typical release's omega.x is negative, giving a
    // negative -- downward -- y term unless the grip's matrix flips the
    // sign). Real backspin (the "rise"/carry a four-seam fastball is
    // known for) needs the OPPOSITE sign, hence -1.0 for the fastball
    // grips below; a curveball's characteristic extra drop is exactly
    // the un-flipped, natural sign, hence +1.0 there.
    GripPreset presets[kGripPresetCount] = {
        // 0: four-seam fastball -- mostly backspin, high efficiency, no
        // speed penalty.
        GripPreset{{-1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0}, Vec3{1.0, 0.0, 0.0}, 1.0, 0.0},
        // 1: two-seam -- similar axis, slightly less efficient transfer
        // (more of the arm's energy stays as speed, per the real pitch's
        // reputation for less "rise").
        GripPreset{{-0.9, 0, 0, 0, 0.9, 0, 0, 0, 0.9}, Vec3{1.0, 0.0, 0.0}, 0.85, 0.0},
        // 2: curveball -- natural (un-flipped) sign for extra drop,
        // suppresses the incidental z-component of hand motion (keeps
        // spin axis cleanly horizontal for a clean 12-6 break), and costs
        // some speed for the extra wrist snap.
        GripPreset{{1.0, 0, 0, 0, 1.0, 0, 0, 0, 0.3}, Vec3{-1.0, 0.0, 0.0}, 1.1, 0.08},
        // 3: slider/gyro -- rotates emphasis toward the travel axis (z),
        // for a bullet-like spin with less transverse Magnus break.
        GripPreset{{0.3, 0, 0, 0, 0.3, 0, 0, 0, 1.0}, Vec3{0.0, 0.0, 1.0}, 0.9, 0.05},
    };
};

}  // namespace sim
