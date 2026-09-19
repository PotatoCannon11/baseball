#include <cmath>
#include <cstdio>

#include "cpu/at_bat.h"
#include "cpu/batter_ai.h"
#include "cpu/pitcher_ai.h"
#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

bool has_nan(const sim::Vec3& v) { return std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z); }

struct PitchResult {
    cpu::PitchOutcome outcome = cpu::PitchOutcome::InProgress;
    cpu::FairBallResult fair;
    bool anomaly = false;
};

PitchResult run_one_pitch(sim::SimState* state, const sim::SimConfig& config, const cpu::PitcherDifficulty& pd,
                           const cpu::BatterDifficulty& bd, const cpu::StrikeZoneBox& zone) {
    cpu::PitcherBrain pitcher_brain{};
    cpu::BatterBrain batter_brain{};
    cpu::reset_for_new_pitch(&batter_brain);

    const sim::Vec3 release_point{0.0, 1.8 + config.arm.virtual_arm_length_m, 18.0};
    cpu::begin_pitch(release_point, zone.center, zone.half_extent, pd, config.arm, state->tick, &state->rng,
                      &pitcher_brain);

    bool released = false;
    std::uint32_t release_tick = 0;

    constexpr int kMaxTicks = 240 * 6;
    for (int i = 0; i < kMaxTicks; ++i) {
        common::PlayerInput pitcher_input{};
        if (pitcher_brain.winding_up) pitcher_input = cpu::tick_pitch(state->tick, &pitcher_brain);

        common::PlayerInput batter_input{};
        if (released) batter_input = cpu::tick_bat(state->tick, batter_brain, config.tick_dt_seconds);

        const sim::Vec3 pos_before = state->ball.position;
        sim::StepEvents events;
        sim::SimInputs inputs{pitcher_input, batter_input};
        *state = sim::step(*state, inputs, config, &events);

        if (has_nan(state->ball.position) || has_nan(state->ball.velocity)) {
            PitchResult r;
            r.anomaly = true;
            return r;
        }

        if (events.ball_released_this_tick) {
            released = true;
            release_tick = state->tick;
            continue;
        }
        if (!released) continue;

        cpu::maybe_decide(*state, release_tick, config.tick_dt_seconds, zone.center, zone.half_extent, bd,
                           &state->rng, &batter_brain);

        if (events.bat_contact_occurred) {
            PitchResult r;
            r.outcome = cpu::classify_contact(state->ball, config, &r.fair);
            return r;
        }

        if (pos_before.z > zone.center.z && state->ball.position.z <= zone.center.z) {
            const bool swung = batter_brain.decided && batter_brain.swinging;
            const bool in_zone = cpu::is_in_zone(state->ball.position, zone);
            PitchResult r;
            r.outcome = cpu::classify_no_contact(swung, in_zone);
            return r;
        }
    }

    PitchResult r;
    r.anomaly = true;
    return r;
}

}  // namespace

// "Headless CPU-vs-CPU mode ... runs thousands of at-bats for validation
// and statistics" -- checked here at a smaller (but still substantial)
// count to keep the test suite fast. Per spec: no NaNs, no stuck states,
// and the outcome/fair-ball distributions should be physically plausible
// (not necessarily realistic-looking yet -- that's milestone 9/10 tuning
// territory -- just bounded and sane).
int main() {
    bool ok = true;

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 99);
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    const cpu::PitcherDifficulty pitcher_difficulty;
    const cpu::BatterDifficulty batter_difficulty;
    const cpu::StrikeZoneBox zone;

    constexpr int kPitchCount = 800;
    int anomalies = 0;
    int outcome_counts[6] = {0, 0, 0, 0, 0, 0};
    int fair_ball_count = 0;
    double exit_velocity_sum = 0.0, exit_velocity_min = 1e18, exit_velocity_max = -1e18;
    double launch_angle_min = 1e18, launch_angle_max = -1e18;
    double carry_distance_min = 1e18, carry_distance_max = -1e18;

    for (int i = 0; i < kPitchCount; ++i) {
        const PitchResult result = run_one_pitch(&state, config, pitcher_difficulty, batter_difficulty, zone);
        if (result.anomaly) {
            ++anomalies;
            continue;
        }
        outcome_counts[static_cast<int>(result.outcome)]++;
        if (result.outcome == cpu::PitchOutcome::FairBall) {
            ++fair_ball_count;
            const double ev_mph = result.fair.exit_velocity_mps * 2.23694;
            exit_velocity_sum += ev_mph;
            exit_velocity_min = std::min(exit_velocity_min, ev_mph);
            exit_velocity_max = std::max(exit_velocity_max, ev_mph);
            launch_angle_min = std::min(launch_angle_min, result.fair.launch_angle_deg);
            launch_angle_max = std::max(launch_angle_max, result.fair.launch_angle_deg);
            carry_distance_min = std::min(carry_distance_min, result.fair.carry_distance_m);
            carry_distance_max = std::max(carry_distance_max, result.fair.carry_distance_m);
        }
        state.ball.position = sim::Vec3{0.0, 1.8, 18.0};
        state.ball.velocity = sim::Vec3{};
        state.ball.angular_velocity = sim::Vec3{};
    }

    std::printf("Ran %d pitches: ball=%d strike=%d swinging_strike=%d foul=%d fair=%d anomalies=%d\n", kPitchCount,
                outcome_counts[static_cast<int>(cpu::PitchOutcome::CalledBall)],
                outcome_counts[static_cast<int>(cpu::PitchOutcome::CalledStrike)],
                outcome_counts[static_cast<int>(cpu::PitchOutcome::SwingingStrike)],
                outcome_counts[static_cast<int>(cpu::PitchOutcome::Foul)], fair_ball_count, anomalies);

    ok &= check(anomalies == 0, "no pitch should NaN or get stuck within the per-pitch tick ceiling");

    const int classified = kPitchCount - anomalies;
    ok &= check(classified == outcome_counts[1] + outcome_counts[2] + outcome_counts[3] + outcome_counts[4] +
                                   outcome_counts[5],
                "every non-anomalous pitch must be classified into exactly one outcome");

    if (fair_ball_count > 0) {
        const double mean_ev = exit_velocity_sum / fair_ball_count;
        std::printf("fair-ball exit velocity: mean=%.1f min=%.1f max=%.1f mph\n", mean_ev, exit_velocity_min,
                    exit_velocity_max);
        std::printf("fair-ball launch angle: min=%.1f max=%.1f deg\n", launch_angle_min, launch_angle_max);
        std::printf("fair-ball carry distance: min=%.1f max=%.1f m\n", carry_distance_min, carry_distance_max);

        // Loose physical sanity bounds -- catching gross errors (e.g. a
        // unit conversion bug giving 10,000 mph exit velocities or
        // negative carry distances), not enforcing realistic averages.
        ok &= check(exit_velocity_min > 0.0 && exit_velocity_max < 250.0,
                    "exit velocities should be physically plausible (0-250 mph)");
        ok &= check(launch_angle_min > -90.0 && launch_angle_max < 90.0, "launch angles should be within +/-90 deg");
        ok &= check(carry_distance_min >= 0.0 && carry_distance_max < 300.0,
                    "carry distances should be physically plausible (under 300 m)");
    } else {
        std::printf("(no fair balls in this batch -- not itself a failure, just unlucky/CPU-tuning-dependent)\n");
    }

    if (ok) {
        std::printf("PASS: CPU-vs-CPU batch ran cleanly with sane statistics\n");
        return 0;
    }
    return 1;
}
