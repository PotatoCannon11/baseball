#pragma once

#include "sim/math.h"

// Ball physical properties, editable (milestone 9 property editor writes
// these from a config file). Defaults are official MLB ball specs
// (radius, mass) and commonly-cited baseball aerodynamics coefficients
// (Adair, "The Physics of Baseball"; Nathan, "The Physics of Baseballs").
namespace sim {

struct BallProperties {
    double mass_kg = 0.145;    // official MLB ball, ~5.125 oz
    double radius_m = 0.0366;  // official MLB ball, ~2.9 in diameter

    // Solid-sphere moment of inertia approximation (I = 2/5 m r^2). A real
    // baseball (cork/rubber core, wound yarn, leather cover) isn't uniform
    // density, but a solid sphere is the standard simplification absent a
    // measured radius-of-gyration for this specific ball model.
    double moment_of_inertia() const { return 0.4 * mass_kg * radius_m * radius_m; }
    double volume_m3() const { return (4.0 / 3.0) * math::kPi * radius_m * radius_m * radius_m; }
    double cross_section_area_m2() const { return math::kPi * radius_m * radius_m; }

    // Drag crisis model: Cd transitions from a subcritical (higher) to a
    // supercritical (lower) value around a critical Reynolds number, per
    // the well-documented "drag crisis" for baseballs (the seams trip the
    // boundary layer, unlike a smooth sphere). Roughness shifts the
    // critical Reynolds number lower (a rougher/more seam-prominent ball
    // trips turbulence sooner), matching the physical mechanism, not just
    // a fit parameter.
    double drag_coefficient_subcritical = 0.50;
    double drag_coefficient_supercritical = 0.30;
    double reynolds_critical_smooth = 1.4e5;  // ~typical cited value for a baseball
    double roughness = 1.0;                   // 1.0 = stock ball; >1 shifts transition earlier
    double reynolds_transition_width = 3.0e4;

    // Magnus lift slope: Cl ~= magnus_lift_slope * spin_factor for
    // spin_factor = r*omega_perp/|v| (Nathan's fit gives slope ~0.62 for
    // spin factor below ~0.5; this sim clamps Cl at magnus_cl_max beyond
    // that since the linear fit breaks down at high spin factor).
    double magnus_lift_slope = 0.62;
    double magnus_cl_max = 0.35;

    // Aerodynamic spin decay: coarse exponential model,
    // domega/dt = -omega/spin_decay_time_s. Cited decay is on the order of
    // a few hundred ball rotations traveling to the plate; this constant
    // is a documented approximation, not a measured value for this engine.
    double spin_decay_time_s = 3.0;

    double coefficient_of_restitution_vs_bat = 0.50;  // ball-bat COR, e (Nathan)
};

struct GroundSurface {
    double restitution = 0.35;
    double static_friction = 0.6;
    double kinetic_friction = 0.4;
    double rolling_resistance = 0.05;  // deceleration coefficient, applied as mu_roll * g
};

namespace ground_presets {
inline constexpr GroundSurface kGrass{0.35, 0.6, 0.45, 0.06};
inline constexpr GroundSurface kTurf{0.45, 0.5, 0.4, 0.03};
inline constexpr GroundSurface kDirt{0.30, 0.7, 0.55, 0.08};
}  // namespace ground_presets

}  // namespace sim
