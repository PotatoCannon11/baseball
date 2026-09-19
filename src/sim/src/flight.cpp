#include "sim/flight.h"

#include <algorithm>

#include "sim/math.h"

namespace sim {

namespace {

// Acceleration purely as a function of velocity and (held-constant) spin;
// doesn't depend on position, so wind is uniform for now (height-dependent
// wind profile is milestone 10 polish).
Vec3 acceleration(const Vec3& velocity, const Vec3& spin, const BallProperties& ball, const Environment& env,
                   double gravity_mps2) {
    const double m = ball.mass_kg;
    const double rho = env.air_density_kgpm3();
    const double volume = ball.volume_m3();

    // Added mass (potential-flow result for a sphere: m_added = 0.5 * rho
    // * V) increases the inertia the aerodynamic/gravity forces must
    // accelerate. Tiny for a baseball (~1e-4 kg vs ~0.145 kg) but cheap and
    // asked for by spec.
    const double m_eff = m + 0.5 * rho * volume;

    Vec3 force{0.0, -m * gravity_mps2, 0.0};       // gravity
    force.y += rho * volume * gravity_mps2;         // buoyancy, upward

    const Vec3 v_rel = velocity - env.wind_mps;
    const double speed = v_rel.length();
    if (speed > 1e-9) {
        const double area = ball.cross_section_area_m2();
        const double diameter = 2.0 * ball.radius_m;
        const double re = rho * speed * diameter / env.dynamic_viscosity_pas;

        // Drag crisis: smoothly (logistic, autodiff-friendly per the
        // "design for a future autodiff tool" guidance) interpolate from
        // the subcritical to supercritical Cd around a roughness-shifted
        // critical Reynolds number.
        const double re_crit = ball.reynolds_critical_smooth / ball.roughness;
        const double sigmoid = 1.0 / (1.0 + math::exp(-(re - re_crit) / ball.reynolds_transition_width));
        const double cd = ball.drag_coefficient_subcritical +
                           (ball.drag_coefficient_supercritical - ball.drag_coefficient_subcritical) * sigmoid;

        const Vec3 v_hat = v_rel * (1.0 / speed);
        const double drag_mag = 0.5 * rho * area * cd * speed * speed;
        force += v_hat * (-drag_mag);

        // Magnus: transverse spin component only (spec: "Magnus force from
        // the transverse spin component only (omega x v)").
        const Vec3 spin_along = v_hat * spin.dot(v_hat);
        const Vec3 spin_perp = spin - spin_along;
        const double spin_perp_mag = spin_perp.length();
        if (spin_perp_mag > 1e-9) {
            const Vec3 cross = spin_perp.cross(v_rel);
            const double cross_mag = cross.length();
            if (cross_mag > 1e-9) {
                const Vec3 lift_dir = cross * (1.0 / cross_mag);
                // Nathan's linear fit: Cl ~= 0.62 * spin_factor for
                // spin_factor = r*omega_perp/v, clamped for high spin.
                const double spin_factor = (ball.radius_m * spin_perp_mag) / speed;
                const double cl = std::min(ball.magnus_lift_slope * spin_factor, ball.magnus_cl_max);
                const double magnus_mag = 0.5 * rho * area * cl * speed * speed;
                force += lift_dir * magnus_mag;
            }
        }
    }

    return force * (1.0 / m_eff);
}

struct Derivative {
    Vec3 dposition;  // = velocity
    Vec3 dvelocity;  // = acceleration
};

Derivative evaluate(const Vec3& velocity, const Vec3& spin, const BallProperties& ball, const Environment& env,
                     double gravity_mps2) {
    return Derivative{velocity, acceleration(velocity, spin, ball, env, gravity_mps2)};
}

}  // namespace

BallState integrate_flight_rk4(const BallState& prev, const BallProperties& ball, const Environment& env,
                                double gravity_mps2, double dt) {
    BallState next = prev;

    const Vec3 spin = prev.angular_velocity;  // held constant across this step's stages, see header comment

    const Derivative k1 = evaluate(prev.velocity, spin, ball, env, gravity_mps2);
    const Derivative k2 = evaluate(prev.velocity + k1.dvelocity * (0.5 * dt), spin, ball, env, gravity_mps2);
    const Derivative k3 = evaluate(prev.velocity + k2.dvelocity * (0.5 * dt), spin, ball, env, gravity_mps2);
    const Derivative k4 = evaluate(prev.velocity + k3.dvelocity * dt, spin, ball, env, gravity_mps2);

    next.position = prev.position + (dt / 6.0) * (k1.dposition + 2.0 * k2.dposition + 2.0 * k3.dposition +
                                                    k4.dposition);
    next.velocity = prev.velocity + (dt / 6.0) * (k1.dvelocity + 2.0 * k2.dvelocity + 2.0 * k3.dvelocity +
                                                    k4.dvelocity);

    // Closed-form exponential spin decay (domega/dt = -omega/tau), see
    // header comment for why this isn't folded into the RK4 state.
    if (ball.spin_decay_time_s > 0.0) {
        next.angular_velocity = prev.angular_velocity * math::exp(-dt / ball.spin_decay_time_s);
    }

    next.orientation = prev.orientation.integrated(next.angular_velocity, dt);

    return next;
}

}  // namespace sim
