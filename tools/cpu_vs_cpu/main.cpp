// Headless CPU-vs-CPU batch runner: "headless CPU-vs-CPU mode (no window,
// no GL) runs thousands of at-bats for validation and statistics." Prints
// exit-velocity distribution, launch angles, foul/strike/ball rates, and
// flags any NaN or stuck-state anomaly.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "cpu/at_bat.h"
#include "cpu/batter_ai.h"
#include "cpu/pitcher_ai.h"
#include "sim/step.h"

namespace {

struct PitchResult {
    cpu::PitchOutcome outcome = cpu::PitchOutcome::InProgress;
    cpu::FairBallResult fair;
    bool anomaly = false;  // NaN or ran past the per-pitch tick ceiling
};

bool has_nan(const sim::Vec3& v) { return std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z); }

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

    constexpr int kMaxTicks = 240 * 6;  // 6 s safety ceiling per pitch
    for (int i = 0; i < kMaxTicks; ++i) {
        common::PlayerInput pitcher_input{};
        if (pitcher_brain.winding_up) {
            pitcher_input = cpu::tick_pitch(state->tick, &pitcher_brain);
        }

        common::PlayerInput batter_input{};
        if (released) {
            batter_input = cpu::tick_bat(state->tick, batter_brain, config.tick_dt_seconds);
        }

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
#ifdef BASEBALL_CPU_VS_CPU_DEBUG
            std::fprintf(stderr, "released at tick=%u pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f) spin=(%.3f,%.3f,%.3f)\n",
                         state->tick, state->ball.position.x, state->ball.position.y, state->ball.position.z,
                         state->ball.velocity.x, state->ball.velocity.y, state->ball.velocity.z,
                         state->ball.angular_velocity.x, state->ball.angular_velocity.y, state->ball.angular_velocity.z);
#endif
            continue;
        }
        if (!released) continue;
#ifdef BASEBALL_CPU_VS_CPU_DEBUG
        if ((state->tick - release_tick) % 40 == 0) {
            std::fprintf(stderr, "  t+%u pos=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f)\n", state->tick - release_tick,
                         state->ball.position.x, state->ball.position.y, state->ball.position.z, state->ball.velocity.x,
                         state->ball.velocity.y, state->ball.velocity.z);
        }
#endif

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
#ifdef BASEBALL_CPU_VS_CPU_DEBUG
            std::fprintf(stderr, "crossing: pos=(%.3f,%.3f,%.3f) swung=%d in_zone=%d decided=%d swing_start=%u contact=%u tick=%u\n",
                         state->ball.position.x, state->ball.position.y, state->ball.position.z, swung, in_zone,
                         batter_brain.decided, batter_brain.swing_start_tick, batter_brain.contact_tick, state->tick);
#endif
            PitchResult r;
            r.outcome = cpu::classify_no_contact(swung, in_zone);
            return r;
        }
    }

    PitchResult r;
    r.anomaly = true;  // never resolved within the ceiling: a stuck state
    return r;
}

const char* outcome_name(cpu::PitchOutcome o) {
    switch (o) {
        case cpu::PitchOutcome::CalledBall: return "called ball";
        case cpu::PitchOutcome::CalledStrike: return "called strike";
        case cpu::PitchOutcome::SwingingStrike: return "swinging strike";
        case cpu::PitchOutcome::Foul: return "foul";
        case cpu::PitchOutcome::FairBall: return "fair ball";
        default: return "in progress (anomaly)";
    }
}

}  // namespace

int main(int argc, char** argv) {
    int pitch_count = 2000;
    std::uint32_t seed = 42;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--pitches") == 0 && i + 1 < argc) {
            pitch_count = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
    }

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, seed);
    state.ball.position = sim::Vec3{0.0, 1.8, 18.0};  // parked at the mound between pitches
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    const cpu::PitcherDifficulty pitcher_difficulty;
    const cpu::BatterDifficulty batter_difficulty;
    const cpu::StrikeZoneBox zone;

    int counts[6] = {0, 0, 0, 0, 0, 0};  // indexed by PitchOutcome, InProgress used for anomalies
    int anomalies = 0;
    std::vector<double> exit_velocities_mph;
    std::vector<double> launch_angles_deg;
    std::vector<double> carry_distances_m;

    for (int i = 0; i < pitch_count; ++i) {
        const PitchResult result = run_one_pitch(&state, config, pitcher_difficulty, batter_difficulty, zone);
        if (result.anomaly) {
            ++anomalies;
            continue;
        }
        counts[static_cast<int>(result.outcome)]++;
        if (result.outcome == cpu::PitchOutcome::FairBall) {
            exit_velocities_mph.push_back(result.fair.exit_velocity_mps * 2.23694);
            launch_angles_deg.push_back(result.fair.launch_angle_deg);
            carry_distances_m.push_back(result.fair.carry_distance_m);
        }
        // Reset the ball back to the mound for the next pitch -- no
        // fielders/baserunning yet, so between-pitch ball placement is
        // just bookkeeping, not physics.
        state.ball.position = sim::Vec3{0.0, 1.8, 18.0};
        state.ball.velocity = sim::Vec3{};
        state.ball.angular_velocity = sim::Vec3{};
    }

    std::printf("Ran %d pitches (seed=%u)\n\n", pitch_count, seed);
    std::printf("Outcome counts:\n");
    for (int o = static_cast<int>(cpu::PitchOutcome::CalledBall); o <= static_cast<int>(cpu::PitchOutcome::FairBall);
         ++o) {
        std::printf("  %-18s %5d  (%.1f%%)\n", outcome_name(static_cast<cpu::PitchOutcome>(o)), counts[o],
                    100.0 * counts[o] / pitch_count);
    }
    std::printf("  %-18s %5d  (%.1f%%)\n", "anomaly/stuck", anomalies, 100.0 * anomalies / pitch_count);

    auto summarize = [](const char* label, const std::vector<double>& v) {
        if (v.empty()) {
            std::printf("%s: no fair balls\n", label);
            return;
        }
        double sum = 0.0, lo = v[0], hi = v[0];
        for (double x : v) {
            sum += x;
            lo = std::min(lo, x);
            hi = std::max(hi, x);
        }
        std::printf("%s: n=%zu mean=%.1f min=%.1f max=%.1f\n", label, v.size(), sum / v.size(), lo, hi);
    };

    std::printf("\nFair-ball statistics:\n");
    summarize("  exit velocity (mph)", exit_velocities_mph);
    summarize("  launch angle (deg)", launch_angles_deg);
    summarize("  carry distance (m)", carry_distances_m);

    if (anomalies > 0) {
        std::fprintf(stderr, "\nFAIL: %d anomalous pitch(es) (NaN or stuck state)\n", anomalies);
        return 1;
    }

    return 0;
}
