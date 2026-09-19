#pragma once

#include <algorithm>

#include "sim/vec3.h"

// Small collision-math primitives shared by the ball-vs-bat and
// ball-vs-ground contact code. Kept dependency-free (no SDL/render) per
// the sim library's isolation rule.
namespace sim::geometry {

struct ClosestSegmentSegmentResult {
    double s = 0.0;  // parameter along segment 1, [0, 1]
    double t = 0.0;  // parameter along segment 2, [0, 1]
    Vec3 point_on_1;
    Vec3 point_on_2;
    double distance_squared = 0.0;
};

// Closest points between two line segments p1->q1 and p2->q2. Standard
// algorithm (Ericson, "Real-Time Collision Detection", section 5.1.9,
// ClosestPtSegmentSegment) -- a closed form that handles the degenerate
// cases (zero-length segments, parallel segments) without iteration, which
// matters here since this runs inside a per-substep contact search and
// must stay branch-simple and allocation-free.
inline ClosestSegmentSegmentResult closest_segment_segment(const Vec3& p1, const Vec3& q1, const Vec3& p2,
                                                             const Vec3& q2) {
    constexpr double kEpsilon = 1e-12;

    const Vec3 d1 = q1 - p1;
    const Vec3 d2 = q2 - p2;
    const Vec3 r = p1 - p2;
    const double a = d1.dot(d1);
    const double e = d2.dot(d2);
    const double f = d2.dot(r);

    double s = 0.0, t = 0.0;

    if (a <= kEpsilon && e <= kEpsilon) {
        // Both segments degenerate to points.
        s = 0.0;
        t = 0.0;
    } else if (a <= kEpsilon) {
        s = 0.0;
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = d1.dot(r);
        if (e <= kEpsilon) {
            t = 0.0;
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = d1.dot(d2);
            const double denom = a * e - b * b;
            if (denom > kEpsilon) {
                s = std::clamp((b * f - c * e) / denom, 0.0, 1.0);
            } else {
                s = 0.0;
            }
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }

    ClosestSegmentSegmentResult result;
    result.s = s;
    result.t = t;
    result.point_on_1 = p1 + d1 * s;
    result.point_on_2 = p2 + d2 * t;
    const Vec3 diff = result.point_on_1 - result.point_on_2;
    result.distance_squared = diff.dot(diff);
    return result;
}

}  // namespace sim::geometry
