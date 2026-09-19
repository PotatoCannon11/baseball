#include <cmath>
#include <cstdio>

#include "sim/flight.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

sim::BallState fly_for(double total_time_s, double dt) {
    sim::BallProperties ball;
    sim::Environment env;
    sim::BallState state{};
    state.position = sim::Vec3{0.0, 30.0, 0.0};
    state.velocity = sim::Vec3{35.0, 5.0, 12.0};
    state.orientation = sim::Quat::identity();
    state.angular_velocity = sim::Vec3{0.0, 0.0, 180.0};

    const int steps = static_cast<int>(total_time_s / dt);
    for (int i = 0; i < steps; ++i) {
        state = sim::integrate_flight_rk4(state, ball, env, 9.80665, dt);
    }
    return state;
}

}  // namespace

// "Convergence: halving the timestep barely changes results." Position/
// velocity are integrated with RK4 (4th-order), but spin decay is applied
// as a closed-form exponential AFTER each RK4 step using that step's
// starting spin (see flight.h) -- a first-order operator split. That
// caps the SCHEME's overall order at 1, so this only checks the literal
// spec requirement (small absolute change) rather than a specific
// convergence rate; the discrepancy should still shrink noticeably as dt
// shrinks, just roughly linearly rather than quartically.
int main() {
    bool ok = true;

    const double total_time = 1.0;
    const double dt_coarse = 1.0 / 240.0;
    const double dt_fine = 1.0 / 480.0;
    const double dt_finer = 1.0 / 960.0;

    const sim::BallState coarse = fly_for(total_time, dt_coarse);
    const sim::BallState fine = fly_for(total_time, dt_fine);
    const sim::BallState finer = fly_for(total_time, dt_finer);

    const double diff_coarse_fine = (coarse.position - fine.position).length();
    const double diff_fine_finer = (fine.position - finer.position).length();

    std::printf("|coarse-fine| = %.8f m, |fine-finer| = %.8f m\n", diff_coarse_fine, diff_fine_finer);

    ok &= check(diff_coarse_fine < 1e-3, "halving the timestep should barely change the resulting position");
    // Further halving should shrink the discrepancy, not grow or repeat
    // it (see the comment above main() for why this isn't checked
    // against a specific convergence order).
    ok &= check(diff_fine_finer < diff_coarse_fine,
                "further halving the timestep should further shrink the discrepancy");

    if (ok) {
        std::printf("PASS: flight integration converges as the timestep shrinks\n");
        return 0;
    }
    return 1;
}
