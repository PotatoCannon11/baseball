#pragma once

#include "sim/ball_properties.h"
#include "sim/bat_geometry.h"
#include "sim/bat_properties.h"
#include "sim/state.h"

// Ball-vs-bat and ball-vs-ground contact. "The bat is kinematic, driven by
// PlayerInput. The ball is integrated ... The contact function must be
// pure and callable from any peer (bat pose stream + ball state in, result
// out)." Nothing here mutates BatState -- the bat's own moment of inertia
// is used only to compute a physically correct effective mass at the
// contact point (see resolve_bat_contact), never to move the bat itself.
namespace sim {

// Time, along a straight-line estimate of the ball's motion this tick,
// closest to a bat-chain collision, in [0, 1] (fraction of dt), or a
// negative value if the swept path never comes within contact range. This
// is a broad/narrow-phase CCD test, not the actual contact resolution --
// used only to decide how much of the tick is free flight before handing
// off to resolve_bat_contact. Straight-line (not curved/RK4) sweep is a
// deliberate approximation: over one tick (~4 ms at 240 Hz) the curvature
// from drag/gravity is negligible relative to bat/ball dimensions.
double time_of_impact_fraction(const Vec3& ball_start, const Vec3& ball_end, double ball_radius,
                                const BatCapsuleChain& chain);

struct ContactOutcome {
    BallState ball;
    bool contact_occurred = false;
    double contact_duration_s = 0.0;
    // Combined system kinetic energy (ball translation + rotation, PLUS
    // the bat's local, non-persisted recoil about its pivot -- see
    // collision.cpp's bat_omega_local) before/after, for the milestone-4
    // energy-audit validation test. The Hunt-Crossley dissipation term
    // (lambda*x^n*xdot) is the only thing allowed to remove energy from
    // this combined total; it should never increase.
    double system_energy_before_j = 0.0;
    double system_energy_after_j = 0.0;

    // Change in the bat's LOCAL (non-persisted) angular velocity over the
    // contact -- exposed for validation (e.g. checking momentum
    // conservation using the effective-mass mechanics) and diagnostics,
    // not used by the sim itself (BatState is never mutated by contact).
    Vec3 bat_angular_velocity_delta;
};

// Resolves a bat-ball contact starting from ball_at_impact (assumed to
// already be at first touch, e.g. via integrate_flight_rk4 up to the CCD
// time-of-impact) using Hunt-Crossley normal contact (F = k*x^n +
// lambda*x^n*xdot) integrated at microsecond substeps, plus tangential
// stick-slip friction with tracked tangential deformation. The bat chain
// and bat kinematics (angular_velocity about pivot_position; pivot itself
// assumed stationary, see bat_pivot_velocity) are held fixed for the
// (sub-millisecond) duration of contact -- the bat's orientation changing
// mid-contact is a second-order effect at this timescale.
ContactOutcome resolve_bat_contact(const BallState& ball_at_impact, const BatCapsuleChain& chain,
                                    const BatState& bat, const BatProperties& bat_props,
                                    const BallProperties& ball_props, double max_duration_s,
                                    const Vec3& bat_pivot_velocity = Vec3{});

// Ball-vs-ground-plane (y=0) contact: Newtonian normal restitution plus
// Coulomb tangential friction with the sliding-to-rolling effective-mass
// reduction (1/(1/m + R^2/I), see collision.cpp for the derivation), and a
// small rolling-resistance deceleration once the ball is resting/rolling.
// free_flight_next is the ball state integrate_flight_rk4 would produce
// with no ground; this function only modifies it if the flight this tick
// crosses y = radius.
BallState apply_ground_contact(const BallState& prev, const BallState& free_flight_next,
                                const BallProperties& ball_props, const GroundSurface& ground, double dt);

}  // namespace sim
