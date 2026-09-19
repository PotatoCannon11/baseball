#include "sim/collision.h"

#include <algorithm>

#include "sim/geometry.h"
#include "sim/math.h"

namespace sim {

namespace {

double kinetic_energy(const Vec3& velocity, const Vec3& angular_velocity, double mass, double moment_of_inertia) {
    return 0.5 * mass * velocity.length_squared() + 0.5 * moment_of_inertia * angular_velocity.length_squared();
}

// Closest point on a single capsule (segment with linearly-interpolated
// radius) to a world-space point, via the degenerate case of
// closest_segment_segment with a zero-length "segment" at the point --
// exactly the point-vs-segment closest point test.
struct PointVsCapsule {
    Vec3 point_on_axis;
    double t = 0.0;         // parameter along the capsule, [0, 1]
    double radius = 0.0;    // interpolated radius at t
    double surface_distance = 0.0;  // |point - axis| - radius (negative = overlapping)
};

PointVsCapsule closest_on_capsule(const Vec3& p, const Capsule& c) {
    const geometry::ClosestSegmentSegmentResult r = geometry::closest_segment_segment(p, p, c.a, c.b);
    PointVsCapsule out;
    out.point_on_axis = r.point_on_2;
    out.t = r.t;
    out.radius = c.radius_a + (c.radius_b - c.radius_a) * r.t;
    out.surface_distance = math::sqrt(r.distance_squared) - out.radius;
    return out;
}

}  // namespace

namespace {

// Signed gap between the ball's surface and the nearest bat capsule
// surface at parameter s along the swept segment (positive = clear,
// <= 0 = touching/penetrating).
double signed_gap_at(const Vec3& ball_start, const Vec3& ball_end, double ball_radius, const BatCapsuleChain& chain,
                      double s) {
    const Vec3 p = ball_start + (ball_end - ball_start) * s;
    double best = 1e9;
    for (int i = 0; i < chain.count; ++i) {
        best = std::min(best, closest_on_capsule(p, chain.segments[i]).surface_distance);
    }
    return best - ball_radius;
}

}  // namespace

double time_of_impact_fraction(const Vec3& ball_start, const Vec3& ball_end, double ball_radius,
                                const BatCapsuleChain& chain) {
    // NOTE: this deliberately does NOT use closest_segment_segment's "s"
    // parameter as the impact time. That parameter is the point of
    // globally closest approach between the two (infinite) lines, which
    // for a ball diving straight down onto a THIN, mostly-colinear-with-
    // its-own-path capsule chain is degenerate: the closest-approach point
    // is the swept segment's own endpoint (s=1) even when the ball
    // crossed into contact range well before the tick ended, letting it
    // tunnel deep into the bat before contact resolution ever starts (see
    // the milestone-4 bug this comment replaced). Instead, sample the
    // actual surface gap along the path and bisect for the first crossing
    // -- cheap (a few hundred capsule-distance evaluations, negligible
    // next to the 240 Hz tick budget) and correct regardless of geometry.
    constexpr int kSamples = 32;
    double prev_s = 0.0;
    if (signed_gap_at(ball_start, ball_end, ball_radius, chain, 0.0) <= 0.0) return 0.0;

    for (int i = 1; i <= kSamples; ++i) {
        const double s = static_cast<double>(i) / kSamples;
        const double gap = signed_gap_at(ball_start, ball_end, ball_radius, chain, s);
        if (gap <= 0.0) {
            double lo = prev_s, hi = s;
            for (int iter = 0; iter < 24; ++iter) {
                const double mid = 0.5 * (lo + hi);
                if (signed_gap_at(ball_start, ball_end, ball_radius, chain, mid) <= 0.0) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            return hi;
        }
        prev_s = s;
    }
    return -1.0;
}

ContactOutcome resolve_bat_contact(const BallState& ball_at_impact, const BatCapsuleChain& chain,
                                    const BatState& bat, const BatProperties& bat_props,
                                    const BallProperties& ball_props, double max_duration_s,
                                    const Vec3& bat_pivot_velocity) {
    ContactOutcome outcome;
    outcome.ball = ball_at_impact;

    const double m_ball = ball_props.mass_kg;
    const double i_ball = ball_props.moment_of_inertia();
    const double i_pivot = bat_props.moment_of_inertia_about_pivot();

    // Microsecond substeps, per spec ("integrated at microsecond substeps
    // during contact, so restitution falls with impact speed"). Real
    // bat-ball contact lasts well under a millisecond; 1 us keeps the very
    // stiff Hunt-Crossley spring (k ~ 4e7 N/m^1.5) numerically stable
    // without needing an implicit solver.
    constexpr double kSubstepDt = 1.0e-6;
    const int max_substeps = static_cast<int>(max_duration_s / kSubstepDt);
    // Safety allowance for the ball to actually reach the surface: the CCD
    // time-of-impact is a straight-line approximation, so ball_at_impact
    // may be a few substeps shy of true contact. Bounded search, not an
    // open-ended loop.
    constexpr int kPreContactSearchSubsteps = 500;  // 0.5 ms

    Vec3 pos = ball_at_impact.position;
    Vec3 vel = ball_at_impact.velocity;
    Vec3 omega = ball_at_impact.angular_velocity;
    Vec3 tangential_deformation{0.0, 0.0, 0.0};

    // Tangential spring stiffness for the stick-slip model: derived from
    // the normal stiffness scaled down, since there's no independently
    // measured tangential stiffness for this contact -- a documented
    // approximation, tuned only by the overall friction coefficients
    // (which bound the resulting force regardless of this constant).
    const double tangential_stiffness = bat_props.contact_stiffness * 0.1;
    const double contact_dissipation = bat_props.contact_dissipation(ball_props.coefficient_of_restitution_vs_bat);

    bool ever_contacted = false;
    double contact_time = 0.0;

    // The bat's angular velocity is allowed to "give" locally during the
    // sub-millisecond contact window (never written back to BatState --
    // next tick's bat pose still comes straight from PlayerInput, per
    // spec). This is what makes the effective-mass-at-contact formula
    // (1/m_eff = 1/m_ball + (r x n)^2/I_pivot) actually do anything: if
    // the bat's contact-point velocity were held perfectly fixed, the
    // ball would see an infinitely stiff (infinite-mass) bat regardless
    // of I_pivot. Letting it recoil locally, driven by the same contact
    // force reacted through Newton's third law, is what reproduces the
    // real bat's finite effective mass at contact.
    Vec3 bat_omega_local = bat.angular_velocity;
    const Vec3 bat_omega_start = bat_omega_local;

    // Phase 1: pre-contact search. The CCD time-of-impact is only
    // approximate, so coast the ball forward (no force) until the geometry
    // says it's actually touching, searching the whole capsule chain each
    // substep since we don't yet know which capsule (if any) it'll hit.
    Vec3 contact_axis_point{};
    bool touching = false;
    int search_i = 0;
    for (; search_i < kPreContactSearchSubsteps; ++search_i) {
        double best_surface_distance = 1e9;
        Vec3 best_axis_point{};
        bool any = false;
        for (int c = 0; c < chain.count; ++c) {
            const PointVsCapsule pc = closest_on_capsule(pos, chain.segments[c]);
            if (pc.surface_distance < best_surface_distance) {
                best_surface_distance = pc.surface_distance;
                best_axis_point = pc.point_on_axis;
                any = true;
            }
        }
        if (!any) break;
        if (ball_props.radius_m - best_surface_distance > 0.0) {
            contact_axis_point = best_axis_point;
            touching = true;
            break;
        }
        pos += vel * kSubstepDt;
        contact_time += kSubstepDt;
    }

    if (touching) {
        // Phase 2: the actual Hunt-Crossley contact. The contact frame
        // (normal n, contact point on the bat axis, and therefore r_bat
        // and r_ball) is frozen at the instant contact begins -- it barely
        // moves over a sub-millisecond contact, and freezing it lets x
        // (penetration) be tracked as a proper RELATIVE coordinate
        // (x += xdot*dt) instead of being re-derived every substep from
        // the ball's absolute position against a capsule chain that never
        // itself moves to reflect the bat's local "give". That
        // inconsistency -- xdot reflecting bat recoil while x didn't --
        // was a real bug: it broke energy conservation even with zero
        // Hunt-Crossley damping (verified by setting lambda=0 and seeing
        // energy still drop, independent of substep size). Tracking x via
        // its own dynamics fixes that by construction.
        const Vec3 to_ball = pos - contact_axis_point;
        const double to_ball_len = to_ball.length();
        const Vec3 n = to_ball_len > 1e-12 ? to_ball * (1.0 / to_ball_len) : Vec3{0.0, 1.0, 0.0};
        const Vec3 r_bat = contact_axis_point - bat.pivot_position;
        const Vec3 r_ball = n * (-ball_props.radius_m);

        // Starting penetration, from the same geometric criterion the
        // search loop just used to detect first touch.
        double best_surface_distance = 1e9;
        for (int c = 0; c < chain.count; ++c) {
            best_surface_distance = std::min(best_surface_distance, closest_on_capsule(pos, chain.segments[c]).surface_distance);
        }
        double x = ball_props.radius_m - best_surface_distance;

        for (int i = 0; max_substeps > 0 && i < max_substeps + (kPreContactSearchSubsteps - search_i); ++i) {
            if (x <= 0.0) break;
            ever_contacted = true;

            const Vec3 bat_contact_velocity = bat_pivot_velocity + bat_omega_local.cross(r_bat);
            const Vec3 ball_contact_velocity = vel + omega.cross(r_ball);
            const Vec3 relative_velocity = ball_contact_velocity - bat_contact_velocity;
            const double xdot = -relative_velocity.dot(n);  // positive while still approaching/compressing

            // Hunt-Crossley (Hunt & Crossley 1975): F = k*x^n + lambda*x^n*xdot.
            const double x_pow_n = math::pow(x, bat_props.contact_exponent);
            double f_normal = bat_props.contact_stiffness * x_pow_n + contact_dissipation * x_pow_n * xdot;
            f_normal = std::max(f_normal, 0.0);  // contact can push, never pull

            // Tangential: relative sliding velocity at the contact patch.
            const Vec3 v_t_vec = relative_velocity - n * relative_velocity.dot(n);

            // Stick-slip: track a small spring deformation; while the
            // spring force stays under the Coulomb limit, the contact
            // sticks (deformation grows with relative sliding velocity);
            // once it would exceed the limit, it slips and the force
            // clamps to kinetic friction, with the deformation
            // re-anchored so there's no discontinuity next substep.
            tangential_deformation += v_t_vec * kSubstepDt;
            Vec3 f_tangential = tangential_deformation * (-tangential_stiffness);
            const double f_tangential_mag = f_tangential.length();
            const double slip_limit = bat_props.friction_static * f_normal;
            if (f_tangential_mag > slip_limit && f_tangential_mag > 1e-12) {
                const double kinetic_limit = bat_props.friction_kinetic * f_normal;
                f_tangential = f_tangential * (kinetic_limit / f_tangential_mag);
                tangential_deformation =
                    tangential_stiffness > 1e-12 ? f_tangential * (-1.0 / tangential_stiffness) : Vec3{};
            }

            const Vec3 f_total = n * f_normal + f_tangential;

            // Newton's second law on the ball (real mass, real inertia).
            vel += f_total * (kSubstepDt / m_ball);
            const Vec3 torque = r_ball.cross(f_tangential);  // normal force passes through the center: no torque
            omega += torque * (kSubstepDt / i_ball);
            pos += vel * kSubstepDt;

            // Newton's third law reaction on the bat's LOCAL (non-persisted)
            // angular velocity -- see the comment above the bat_omega_local
            // declaration.
            if (i_pivot > 1e-12) {
                const Vec3 reaction_torque = r_bat.cross(f_total * -1.0);
                bat_omega_local += reaction_torque * (kSubstepDt / i_pivot);
            }

            x += xdot * kSubstepDt;
            contact_time += kSubstepDt;
        }
    }

    outcome.contact_occurred = ever_contacted;
    outcome.contact_duration_s = contact_time;
    outcome.ball.position = pos;
    outcome.ball.velocity = vel;
    outcome.ball.angular_velocity = omega;
    outcome.ball.orientation = ball_at_impact.orientation.integrated(omega, contact_time);

    const double bat_ke_before = i_pivot > 0.0 ? 0.5 * i_pivot * bat_omega_start.length_squared() : 0.0;
    const double bat_ke_after = i_pivot > 0.0 ? 0.5 * i_pivot * bat_omega_local.length_squared() : 0.0;
    outcome.system_energy_before_j =
        kinetic_energy(ball_at_impact.velocity, ball_at_impact.angular_velocity, m_ball, i_ball) + bat_ke_before;
    outcome.system_energy_after_j = kinetic_energy(vel, omega, m_ball, i_ball) + bat_ke_after;
    outcome.bat_angular_velocity_delta = bat_omega_local - bat_omega_start;

    return outcome;
}

BallState apply_ground_contact(const BallState& prev, const BallState& free_flight_next,
                                const BallProperties& ball_props, const GroundSurface& ground, double dt) {
    const double r = ball_props.radius_m;
    const bool crossed = (prev.position.y - r >= 0.0) && (free_flight_next.position.y - r < 0.0);

    if (!crossed) {
        // Rolling resistance for a ball already resting/rolling on the
        // ground (small horizontal deceleration proportional to normal
        // load; g is folded into the coefficient per the usual
        // F_roll = mu_roll * m * g convention).
        BallState next = free_flight_next;
        if (next.position.y - r < 1e-4 && math::fabs(next.velocity.y) < 0.05) {
            const double speed = math::sqrt(next.velocity.x * next.velocity.x + next.velocity.z * next.velocity.z);
            if (speed > 1e-9) {
                const double decel = ground.rolling_resistance * 9.80665 * dt;
                const double new_speed = std::max(0.0, speed - decel);
                const double scale = new_speed / speed;
                next.velocity.x *= scale;
                next.velocity.z *= scale;
            }
            next.position.y = r;
        }
        return next;
    }

    // Interpolate the crossing time within this tick (linear estimate,
    // same rationale as the bat CCD sweep: curvature over one tick is
    // negligible next to the crossing-time precision this needs).
    const double denom = (prev.position.y - free_flight_next.position.y);
    const double t_frac = denom > 1e-12 ? (prev.position.y - r) / denom : 0.0;
    const double t_impact = std::clamp(t_frac, 0.0, 1.0) * dt;

    // Re-integrating flight only up to the impact instant would need the
    // BallProperties/Environment this function doesn't have (ground
    // contact intentionally only needs BallProperties for mass/radius/
    // inertia) -- instead, linearly blend prev/free_flight_next, which is
    // accurate to the same order as the crossing-time estimate itself
    // over a single ~4 ms tick.
    BallState pre_impact = prev;
    pre_impact.position = prev.position + (free_flight_next.position - prev.position) * (t_impact / dt);
    pre_impact.velocity = prev.velocity + (free_flight_next.velocity - prev.velocity) * (t_impact / dt);
    pre_impact.angular_velocity = prev.angular_velocity;
    pre_impact.position.y = r;

    const Vec3 n{0.0, 1.0, 0.0};
    const double vn0 = pre_impact.velocity.dot(n);

    Vec3 vel = pre_impact.velocity;
    Vec3 omega = pre_impact.angular_velocity;

    if (vn0 < 0.0) {
        const double j_n = -ball_props.mass_kg * (1.0 + ground.restitution) * vn0;
        vel += n * (j_n / ball_props.mass_kg);

        // Tangential impulse: sliding-to-rolling transition via the
        // sphere effective mass 1/(1/m + R^2/I) (see collision.h; for a
        // solid sphere this is m/3.5, the standard result behind
        // "a sliding sphere stops slipping at 5/7 its initial speed").
        const Vec3 r_contact{0.0, -r, 0.0};
        const Vec3 contact_velocity = pre_impact.velocity + pre_impact.angular_velocity.cross(r_contact);
        const Vec3 v_t = contact_velocity - n * contact_velocity.dot(n);
        const double v_t_mag = v_t.length();
        if (v_t_mag > 1e-9) {
            const double i_ball = ball_props.moment_of_inertia();
            const double inv_m_eff_t = (1.0 / ball_props.mass_kg) + (r * r) / i_ball;
            const double m_eff_t = 1.0 / inv_m_eff_t;
            const double j_t_full = m_eff_t * v_t_mag;
            const double mu = ground.kinetic_friction;
            const double j_t = std::min(j_t_full, mu * j_n);
            const Vec3 j_t_vec = v_t * (-j_t / v_t_mag);

            vel += j_t_vec * (1.0 / ball_props.mass_kg);
            omega += r_contact.cross(j_t_vec) * (1.0 / i_ball);
        }
    }

    BallState next = pre_impact;
    next.velocity = vel;
    next.angular_velocity = omega;
    next.orientation = pre_impact.orientation.integrated(omega, dt - t_impact);
    // Remaining free-flight time (dt - t_impact) after the bounce is not
    // separately re-integrated here (ground contact is resolved as an
    // instantaneous impulse at t_impact) -- a documented simplification;
    // at 240 Hz the leftover fraction of a tick is a few milliseconds at
    // most and the next tick's flight integration picks up from here.
    next.position = pre_impact.position;
    return next;
}

}  // namespace sim
