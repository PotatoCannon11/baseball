#pragma once

#include "sim/math.h"

// Plain double-precision 3-vector. POD, trivially copyable -- part of
// SimState's byte layout, so no hidden padding or vtable.
namespace sim {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }

    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    double length_squared() const { return dot(*this); }
    double length() const { return math::sqrt(length_squared()); }
    Vec3 normalized() const {
        const double len = length();
        return len > 0.0 ? (*this) * (1.0 / len) : Vec3{0, 0, 0};
    }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

}  // namespace sim
