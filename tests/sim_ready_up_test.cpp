#include <cstdio>

#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// Milestone 7: "a pitch cycle starts when both bound humans signal ready
// ... a fixed, deterministic timeout applies so nobody can stall forever.
// This state lives in SimState."
int main() {
    bool ok = true;

    sim::SimConfig config;
    config.ready_timeout_ticks = 100;  // small, so the timeout path is fast to test

    // --- Case 1: both-ready edge fires exactly once, latched independently ---
    {
        sim::SimState state{};
        state.ball.orientation = sim::Quat::identity();
        state.bat.orientation = sim::Quat::identity();
        state.pitcher_arm.orientation = sim::Quat::identity();

        sim::SimInputs inputs{};
        sim::StepEvents events;

        // Batter signals ready first; pitcher not yet -- no event.
        inputs.batter.buttons = static_cast<std::uint16_t>(common::InputButton::kReady);
        state = sim::step(state, inputs, config, &events);
        ok &= check(!events.ready_for_next_pitch, "one role readying alone should not fire the event");
        ok &= check(state.ready.batter_ready && !state.ready.pitcher_ready,
                    "batter's ready bit should latch even after the button-holding tick passes");

        // Batter releases the button (still latched from before); pitcher
        // now readies -- both true this tick, event should fire.
        inputs.batter.buttons = 0;
        inputs.pitcher.buttons = static_cast<std::uint16_t>(common::InputButton::kReady);
        const std::uint32_t tick_before = state.tick;
        state = sim::step(state, inputs, config, &events);
        ok &= check(events.ready_for_next_pitch, "both roles ready (even across different ticks) should fire once");
        ok &= check(!state.ready.pitcher_ready && !state.ready.batter_ready,
                    "ready flags should reset immediately after firing");
        ok &= check(state.ready.wait_start_tick == state.tick, "the next wait window should start at this tick");
        ok &= check(state.tick == tick_before + 1, "step should still advance exactly one tick");

        // Next tick, nobody readies -- should not immediately refire.
        inputs.pitcher.buttons = 0;
        state = sim::step(state, inputs, config, &events);
        ok &= check(!events.ready_for_next_pitch, "the event should not refire until the next window completes");
    }

    // --- Case 2: timeout fires the event even if nobody ever readies ---
    {
        sim::SimState state{};
        state.ball.orientation = sim::Quat::identity();
        state.bat.orientation = sim::Quat::identity();
        state.pitcher_arm.orientation = sim::Quat::identity();

        const sim::SimInputs inputs{};  // nobody ever signals ready
        sim::StepEvents events;
        bool fired = false;
        std::uint32_t fired_at_tick = 0;
        for (std::uint32_t i = 0; i < config.ready_timeout_ticks + 5; ++i) {
            state = sim::step(state, inputs, config, &events);
            if (events.ready_for_next_pitch) {
                fired = true;
                fired_at_tick = state.tick;
                break;
            }
        }
        ok &= check(fired, "the timeout must fire the event even if neither role ever signals ready");
        ok &= check(fired_at_tick == config.ready_timeout_ticks,
                    "the timeout should fire on exactly the configured tick count, deterministically");
    }

    if (ok) {
        std::printf("PASS: ready-up state is deterministic, edge-triggered, and timeout-bounded\n");
        return 0;
    }
    return 1;
}
