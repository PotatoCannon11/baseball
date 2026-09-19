#pragma once

#include "sim/vec3.h"

// Plain double-precision quaternion, (w, x, y, z). POD, trivially copyable.
namespace sim {

struct Quat {
    double w = 1.0, x = 0.0, y = 0.0, z = 0.0;

    static Quat identity() { return Quat{1, 0, 0, 0}; }

    Quat operator*(const Quat& o) const {
        return Quat{
            w * o.w - x * o.x - y * o.y - z * o.z,
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
        };
    }

    double length_squared() const { return w * w + x * x + y * y + z * z; }
    double length() const { return math::sqrt(length_squared()); }
    Quat normalized() const {
        const double len = length();
        if (len <= 0.0) return identity();
        const double inv = 1.0 / len;
        return Quat{w * inv, x * inv, y * inv, z * inv};
    }
    Quat conjugate() const { return Quat{w, -x, -y, -z}; }

    // Rotates v by this quaternion (v' = q * v * q^-1, expanded without
    // constructing a pure-vector quaternion intermediate).
    Vec3 rotate(const Vec3& v) const {
        const Vec3 u{x, y, z};
        const Vec3 uv = u.cross(v);
        const Vec3 uuv = u.cross(uv);
        return v + (uv * (2.0 * w)) + (uuv * 2.0);
    }

    // Integrates this quaternion forward by angular velocity omega
    // (rad/s, world or local frame consistently with how it's used) over
    // dt seconds, via the standard dq/dt = 0.5 * omega_quat * q update,
    // then renormalizes. This is the same integration scheme used by
    // input::MadgwickFilter, just in double for sim state.
    Quat integrated(const Vec3& omega, double dt) const {
        const Quat omega_q{0.0, omega.x, omega.y, omega.z};
        const Quat qdot = omega_q * (*this);
        Quat result{
            w + 0.5 * dt * qdot.w,
            x + 0.5 * dt * qdot.x,
            y + 0.5 * dt * qdot.y,
            z + 0.5 * dt * qdot.z,
        };
        return result.normalized();
    }
};

}  // namespace sim
