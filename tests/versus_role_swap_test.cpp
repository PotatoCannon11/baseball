#include <cmath>
#include <cstdio>

#include "sim/hash.h"
#include "sim/step.h"
#include "versus/role_assignment.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

// A distinguishable, deterministic-but-not-identical synthetic input
// stream per slot, so pre- vs post-swap sim behavior actually differs
// (if it didn't, a swap bug that silently no-ops the role mapping
// wouldn't be caught by anything below).
common::PlayerInput synthetic_input(input::PlayerSlot slot, std::uint32_t tick) {
    common::PlayerInput in{};
    in.tick = tick;
    in.orientation[3] = static_cast<std::int16_t>(common::PlayerInput::kQuatScale);
    const float base = (slot == input::PlayerSlot::kP1) ? 3.0f : -5.0f;
    const float omega = base * std::sin(static_cast<float>(tick) * 0.01f);
    in.angular_velocity[0] = static_cast<std::int16_t>(omega * common::PlayerInput::kAngVelScale);
    return in;
}

bool has_nan(const sim::SimState& s) {
    const auto bad = [](double v) { return std::isnan(v); };
    return bad(s.ball.position.x) || bad(s.ball.position.y) || bad(s.ball.position.z) || bad(s.ball.velocity.x) ||
           bad(s.ball.velocity.y) || bad(s.ball.velocity.z);
}

// Runs a full session applying a role swap at swap_tick, returning the
// final state and its hash at a fixed checkpoint tick.
struct RunResult {
    sim::SimState final_state{};
    std::uint64_t checkpoint_hash = 0;
};

RunResult run_session(std::uint32_t swap_tick, std::uint32_t checkpoint_tick, std::uint32_t total_ticks) {
    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 7);
    state.ball.position = sim::Vec3{5.0, 1.0, 0.0};  // parked away from the bat/arm, not what's under test
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    versus::RoleAssignment assignment;  // starts P1=pitcher, P2=batter
    RunResult result;

    for (std::uint32_t tick = 1; tick <= total_ticks; ++tick) {
        if (tick == swap_tick) assignment.swap();

        const common::PlayerInput p1_raw = synthetic_input(input::PlayerSlot::kP1, tick);
        const common::PlayerInput p2_raw = synthetic_input(input::PlayerSlot::kP2, tick);

        sim::SimInputs inputs{};
        inputs.pitcher = (assignment.pitcher_slot == input::PlayerSlot::kP1) ? p1_raw : p2_raw;
        inputs.batter = (assignment.batter_slot == input::PlayerSlot::kP1) ? p1_raw : p2_raw;

        state = sim::step(state, inputs, config);
        if (tick == checkpoint_tick) result.checkpoint_hash = sim::hash_state(state);
    }
    result.final_state = state;
    return result;
}

}  // namespace

// Milestone 7 validation test: "a role swap mid-session keeps SimState
// valid and hash-stable."
int main() {
    bool ok = true;

    versus::RoleAssignment assignment;
    ok &= check(assignment.slot_for(versus::Role::kPitcher) == input::PlayerSlot::kP1,
                "default assignment: P1 pitches");
    ok &= check(assignment.slot_for(versus::Role::kBatter) == input::PlayerSlot::kP2, "default assignment: P2 bats");
    assignment.swap();
    ok &= check(assignment.slot_for(versus::Role::kPitcher) == input::PlayerSlot::kP2,
                "after swap: P2 pitches");
    ok &= check(assignment.role_for(input::PlayerSlot::kP1) == versus::Role::kBatter,
                "after swap: P1's role_for lookup agrees (now batting)");

    // should_swap_role: pure scheduling logic.
    versus::RoleScheduleConfig manual;
    manual.mode = versus::SwapMode::kManual;
    ok &= check(!versus::should_swap_role(manual, 999), "manual mode never auto-swaps");

    versus::RoleScheduleConfig every3;
    every3.mode = versus::SwapMode::kEveryNAtBats;
    every3.n_at_bats = 3;
    ok &= check(!versus::should_swap_role(every3, 2), "every-N: not yet at N");
    ok &= check(versus::should_swap_role(every3, 3), "every-N: fires at exactly N");
    ok &= check(versus::should_swap_role(every3, 4), "every-N: stays true past N (caller resets the counter)");

    // Two independent, identically-scheduled sessions (same seed, same
    // per-slot input streams, same swap tick) must produce identical
    // hashes both mid-session (right after the swap has taken effect)
    // and at the end -- the swap itself must be perfectly deterministic,
    // and SimState must remain valid (no NaNs) throughout.
    constexpr std::uint32_t kSwapTick = 500;
    constexpr std::uint32_t kCheckpointTick = 550;
    constexpr std::uint32_t kTotalTicks = 1000;

    const RunResult a = run_session(kSwapTick, kCheckpointTick, kTotalTicks);
    const RunResult b = run_session(kSwapTick, kCheckpointTick, kTotalTicks);

    ok &= check(!has_nan(a.final_state), "SimState must stay NaN-free through a mid-session role swap");
    ok &= check(a.checkpoint_hash == b.checkpoint_hash,
                "hash right after a role swap must be identical across two identically-scheduled runs");
    ok &= check(sim::hash_state(a.final_state) == sim::hash_state(b.final_state),
                "final hash must be identical across two identically-scheduled runs, swap included");

    // A run that swaps AFTER the checkpoint tick (instead of before it)
    // should diverge AT the checkpoint -- confirms the swap actually
    // changes which input drives which role, rather than being a silent
    // no-op. (The bat/pitcher-arm fields are purely instantaneous
    // decodes of that tick's input, not integrated state, so comparing
    // FINAL hashes wouldn't catch a no-op swap: by the last tick, both
    // schedules have long since finished swapping and agree again. The
    // checkpoint, sitting between the two swap ticks, is where the two
    // schedules actually disagree about which role each slot holds.)
    const RunResult c = run_session(kCheckpointTick + 50, kCheckpointTick, kTotalTicks);
    ok &= check(a.checkpoint_hash != c.checkpoint_hash,
                "a swap that has vs. hasn't happened yet by the checkpoint tick must produce different state -- "
                "the swap must actually do something");

    if (ok) {
        std::printf("PASS: role swap is deterministic and keeps SimState valid\n");
        return 0;
    }
    return 1;
}
