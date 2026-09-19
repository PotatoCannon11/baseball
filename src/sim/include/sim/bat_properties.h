#pragma once

#include <cstddef>

#include "sim/math.h"

// Bat geometry and material properties. The profile (radius vs. distance
// from the knob, along the bat's local +Y axis) is the SAME data the
// renderer's lathe mesh is built from (render::LatheProfilePoint has an
// identical layout) -- per spec, "The render mesh and collision chain come
// from the same data." Kept as a plain fixed-size array (no heap) since
// SimConfig must stay allocation-free to load and cheap to copy.
namespace sim {

struct BatProfilePoint {
    double y = 0.0;       // meters from the knob, along the bat's local axis
    double radius = 0.0;  // meters
};

inline constexpr int kMaxBatProfilePoints = 16;

struct BatProperties {
    BatProfilePoint profile[kMaxBatProfilePoints] = {
        {0.000, 0.020}, {0.050, 0.016}, {0.150, 0.014}, {0.350, 0.016},
        {0.550, 0.022}, {0.700, 0.032}, {0.800, 0.035}, {0.860, 0.034}, {0.864, 0.004},
    };
    int profile_count = 9;

    // Wood (ash) density; used to derive mass/center-of-mass/moment of
    // inertia from the profile geometry below rather than hand-specifying
    // them separately, so the two can never drift out of sync with the
    // visible bat shape.
    double density_kgpm3 = 670.0;

    // Hunt-Crossley compliant contact model (Hunt & Crossley 1975):
    // F = k*x^n + lambda*x^n*xdot, x = penetration depth. contact_stiffness
    // is tuned to give a wood-bat-like contact stiffness (very stiff --
    // real bat-ball contact lasts under a millisecond) without requiring
    // sub-microsecond substeps to stay stable; see collision.cpp for the
    // substep count this implies.
    double contact_stiffness = 4.0e7;  // k, N/m^n
    double contact_exponent = 1.5;     // n (Hertzian contact model exponent)

    // The Hunt-Crossley damping coefficient (lambda) isn't itself an
    // intuitive editable property, so it's derived from a target ball-bat
    // coefficient of restitution AT a reference impact speed, via the
    // standard approximate relation (Hunt & Crossley 1975, as summarized
    // in Marhefka & Orin 1999, "A Compliant Contact Model with Nonlinear
    // Damping for Simulation of Robotic Systems"):
    //   lambda ~= 3*k*(1 - e_ref) / (2*v_ref)
    // Because lambda, once derived, stays fixed for every collision
    // regardless of that collision's actual impact speed, an impact
    // faster than v_ref sees a LOWER effective restitution than e_ref and
    // a slower one sees a higher one -- this is exactly the spec's
    // "restitution falls with impact speed," not a bug.
    // ~30 m/s approximates the conditions real ball-bat COR measurements
    // are typically quoted at (e.g. Nathan's COR tests: roughly a 60 mph
    // pitch onto a stationary/clamped bat). Verified against the spec's
    // own worked example (a 90 mph pitch + 70 mph swing giving "roughly
    // 100 mph" exit speed) via tests/sim_collision_efficiency_test.cpp.
    double reference_impact_speed_mps = 30.0;

    double contact_dissipation(double restitution_at_reference) const {
        if (reference_impact_speed_mps <= 0.0) return 0.0;
        return (3.0 * contact_stiffness * (1.0 - restitution_at_reference)) / (2.0 * reference_impact_speed_mps);
    }

    double friction_static = 0.5;
    double friction_kinetic = 0.4;

    // Total mass (integrating the profile as a stack of thin disks of the
    // given density). Each disk's own contribution to a transverse moment
    // of inertia is neglected (thin-disk-about-its-own-axis term, small
    // relative to the parallel-axis term for a bat's aspect ratio) -- a
    // documented simplification, not an exact rigid-body integral.
    double mass_kg() const {
        double m = 0.0;
        for (int i = 0; i + 1 < profile_count; ++i) {
            m += segment_mass(i);
        }
        return m;
    }

    // Distance from the knob (y=0, the pivot) to the center of mass, along
    // the bat's local axis.
    double center_of_mass_y() const {
        double m_total = 0.0;
        double moment = 0.0;
        for (int i = 0; i + 1 < profile_count; ++i) {
            const double m = segment_mass(i);
            const double y_mid = 0.5 * (profile[i].y + profile[i + 1].y);
            m_total += m;
            moment += m * y_mid;
        }
        return m_total > 0.0 ? moment / m_total : 0.0;
    }

    // Moment of inertia about the knob-end pivot (the fixed point the bat
    // swings about), via parallel-axis sum of each segment's point mass at
    // its midpoint distance -- this is exactly the quantity the
    // collision's "effective mass at contact" formula needs (I about the
    // pivot the bat actually rotates around during a swing).
    double moment_of_inertia_about_pivot() const {
        double i_total = 0.0;
        for (int i = 0; i + 1 < profile_count; ++i) {
            const double m = segment_mass(i);
            const double y_mid = 0.5 * (profile[i].y + profile[i + 1].y);
            i_total += m * y_mid * y_mid;
        }
        return i_total;
    }

private:
    // Mass of the conical-frustum segment between profile[i] and
    // profile[i+1], via the frustum volume formula
    // V = (pi*h/3)*(r0^2 + r0*r1 + r1^2).
    double segment_mass(int i) const {
        const double h = profile[i + 1].y - profile[i].y;
        const double r0 = profile[i].radius;
        const double r1 = profile[i + 1].radius;
        const double volume = (math::kPi * h / 3.0) * (r0 * r0 + r0 * r1 + r1 * r1);
        return volume * density_kgpm3;
    }
};

}  // namespace sim
