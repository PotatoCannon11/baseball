#pragma once

#include "sim/bat_properties.h"

// Test-only helper: the bat's radius at a given distance from the knob,
// via the same linear interpolation build_bat_capsule_chain's segments
// use. Lets collision-scenario tests place the ball just outside contact
// range (rather than guessing a fixed offset that happens to be smaller
// than the bat's actual radius at some profile points, which starts the
// ball already deeply overlapping the bat -- a real bug this helper
// exists to avoid repeating).
inline double bat_radius_at_y(const sim::BatProperties& props, double y) {
    for (int i = 0; i + 1 < props.profile_count; ++i) {
        const auto& a = props.profile[i];
        const auto& b = props.profile[i + 1];
        if (y >= a.y && y <= b.y) {
            const double t = (b.y - a.y) > 0.0 ? (y - a.y) / (b.y - a.y) : 0.0;
            return a.radius + (b.radius - a.radius) * t;
        }
    }
    return props.profile[props.profile_count - 1].radius;
}
