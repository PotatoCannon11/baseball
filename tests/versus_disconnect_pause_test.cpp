#include <cstdio>

#include "sim/hash.h"
#include "versus/join_flow.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// Milestone 7 validation test: "a simulated controller disconnect pauses
// on a tick boundary and resumes cleanly." "Simulated" here means exactly
// what it says -- JoinFlow's transitions are called directly, since this
// project has no real Joy-Con to physically unplug (same constraint as
// every other hardware-adjacent test in this suite).
int main() {
    bool ok = true;

    versus::JoinFlow flow;
    ok &= check(flow.state(input::PlayerSlot::kP1) == versus::SlotState::kUnclaimed,
                "an unjoined slot starts kUnclaimed");
    ok &= check(!flow.should_pause(), "nothing is bound yet -- should not pause");

    flow.mark_claimed(input::PlayerSlot::kP1);
    flow.mark_claimed(input::PlayerSlot::kP2);
    ok &= check(!flow.should_pause(), "both slots bound and connected -- should not pause");

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 3);
    state.ball.position = sim::Vec3{5.0, 1.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    const sim::SimInputs inputs{};

    // Run normally for a while.
    for (int i = 0; i < 10; ++i) state = versus::step_or_pause(state, inputs, config, flow);
    const std::uint32_t tick_before_disconnect = state.tick;
    const std::uint64_t hash_before_disconnect = sim::hash_state(state);
    ok &= check(tick_before_disconnect == 10, "ten normal steps should advance the tick by exactly ten");

    // --- Disconnect: P2's controller drops mid-play. ---
    flow.mark_disconnected(input::PlayerSlot::kP2);
    ok &= check(flow.state(input::PlayerSlot::kP2) == versus::SlotState::kDisconnected,
                "P2 should be marked disconnected");
    ok &= check(flow.should_pause(), "any bound slot dropping should signal pause");

    // "Pause deterministically at the next tick boundary": repeated
    // step_or_pause calls while paused must not advance the tick at all,
    // and must not silently synthesize input for the missing player --
    // checked here by confirming the state is byte-identical (same hash)
    // to right before the disconnect, not just "same tick number."
    for (int i = 0; i < 5; ++i) state = versus::step_or_pause(state, inputs, config, flow);
    ok &= check(state.tick == tick_before_disconnect, "the tick must not advance at all while paused");
    ok &= check(sim::hash_state(state) == hash_before_disconnect,
                "the paused state must be byte-identical to the tick right before the disconnect -- "
                "never let a missing device inject zeros into a live pitch");

    // --- Reconnect: resumes cleanly. ---
    flow.mark_reconnected(input::PlayerSlot::kP2);
    ok &= check(flow.state(input::PlayerSlot::kP2) == versus::SlotState::kActive, "P2 should be active again");
    ok &= check(!flow.should_pause(), "no slot is disconnected anymore -- should not pause");

    for (int i = 0; i < 10; ++i) state = versus::step_or_pause(state, inputs, config, flow);
    ok &= check(state.tick == tick_before_disconnect + 10, "ticking should resume exactly where it paused");

    // --- Alternative: instead of reconnecting, the player switches to CPU. ---
    versus::JoinFlow flow2;
    flow2.mark_claimed(input::PlayerSlot::kP1);
    flow2.mark_claimed(input::PlayerSlot::kP2);
    flow2.mark_disconnected(input::PlayerSlot::kP2);
    ok &= check(flow2.should_pause(), "still paused right after disconnect");
    flow2.release(input::PlayerSlot::kP2);  // "let the player switch to a CPU"
    ok &= check(flow2.state(input::PlayerSlot::kP2) == versus::SlotState::kUnclaimed,
                "releasing a disconnected slot should free it (e.g. to be reclaimed by a CPU driver)");
    ok &= check(!flow2.should_pause(), "releasing the disconnected slot should unpause");

    if (ok) {
        std::printf("PASS: a simulated disconnect pauses on a tick boundary and resumes cleanly\n");
        return 0;
    }
    return 1;
}
