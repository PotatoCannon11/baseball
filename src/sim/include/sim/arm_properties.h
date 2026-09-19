#pragma once

// Throwing-arm physical properties. "The arm is a pivoting rod. Release
// speed = arm angular velocity x virtual arm length" plus the mass/size
// effort-scaling formula: "v = v_measured * sqrt((M_arm + m_ref) /
// (M_arm + m)), with M_arm exposed as a tunable."
namespace sim {

struct ArmProperties {
    // Shoulder-to-release-point lever arm. Deliberately much longer than
    // a literal forearm: PlayerInput quantizes angular velocity for a
    // real Joy-Con's documented +/-2000 deg/s (~34.9 rad/s) gyro range
    // (see common/player_input.h's kAngVelScale), and hand_velocity =
    // omega x virtual_arm_length_m must reach real pitching speeds
    // (~40 m/s for 90 mph) within that same representable range --
    // 0.65 m (a literal forearm) would need >60 rad/s, well past what
    // the format (or a real gyro) can even represent, silently clamping
    // and crushing release speed (found via cpu_vs_cpu's sanity output
    // showing release speed far below the requested value). This models
    // the reality that real pitching speed comes from a whole kinetic
    // chain (legs/hips/trunk/shoulder/wrist), not wrist rotation alone;
    // treat this as that chain's effective radius, not a literal arm
    // length -- it isn't meant to be rendered literally.
    double virtual_arm_length_m = 1.3;

    // The thrower's own effective mass/inertia as felt by the ball at
    // release -- large relative to any ball, so a slightly heavier ball
    // barely slows the *arm's motion* itself, but per the spec's formula
    // still measurably reduces how much of that motion becomes ball
    // speed. Exposed as a tunable per spec, not derived from anything.
    double M_arm_kg = 7.0;

    // The ball mass the player's REAL, measured arm speed is implicitly
    // calibrated against -- official MLB ball mass, so a stock ball gives
    // v == v_measured exactly (no adjustment).
    double reference_ball_mass_kg = 0.145;

    // "The stick offsets finger contact point": how far the stick's unit
    // offset moves the effective finger contact point, as a fraction of
    // the ball's radius.
    double finger_offset_scale = 0.6;

    // "Spin = finger impulse x r / I ... capped by maximum finger
    // impulse." The impulse itself scales with release speed (a faster
    // snap plausibly imparts more finger force) up to this cap, N*s.
    double finger_impulse_max = 0.35;
    double finger_impulse_per_mps = 0.01;
};

}  // namespace sim
