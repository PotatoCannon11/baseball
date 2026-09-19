#include <cstdio>

#include "sim/hash.h"
#include "sim/snapshot_ring.h"
#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// "Snapshots: save/load are a memcpy of SimState... Snapshot round-trip:
// save, run N ticks, load, rerun, hashes equal."
int main() {
    bool ok = true;

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 7);
    state.ball.position = sim::Vec3{1.5, 2.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    const sim::SimInputs inputs{};

    // Run 50 ticks to get to a non-trivial state before snapshotting.
    for (int i = 0; i < 50; ++i) state = sim::step(state, inputs, config);

    sim::SnapshotRing ring;
    ring.push(state);  // "save" -- SimState is trivially copyable, this is exactly a memcpy
    const sim::SimState saved = ring.back();
    ok &= check(saved.tick == state.tick, "snapshot push/back should round-trip the exact state");

    // Run N more ticks from the live state.
    constexpr int kN = 200;
    sim::SimState first_run = state;
    for (int i = 0; i < kN; ++i) first_run = sim::step(first_run, inputs, config);

    // "load" the snapshot and rerun the same N ticks.
    sim::SimState second_run = saved;
    for (int i = 0; i < kN; ++i) second_run = sim::step(second_run, inputs, config);

    ok &= check(first_run.tick == second_run.tick, "both runs from the same snapshot should reach the same tick");
    ok &= check(sim::hash_state(first_run) == sim::hash_state(second_run),
                "rerunning N ticks from a saved snapshot must hash identically to the original run");

    // Ring buffer itself: push more than capacity, confirm it still
    // reports the correct (most recent) state and doesn't corrupt data.
    sim::SnapshotRing overflow_ring;
    sim::SimState s = state;
    for (std::size_t i = 0; i < sim::kSnapshotRingCapacity + 10; ++i) {
        s = sim::step(s, inputs, config);
        overflow_ring.push(s);
    }
    ok &= check(overflow_ring.size() == sim::kSnapshotRingCapacity, "ring should saturate at its capacity");
    ok &= check(overflow_ring.back().tick == s.tick, "most recent snapshot should still be the last one pushed");

    if (ok) {
        std::printf("PASS: snapshot save/rerun round-trips to an identical hash\n");
        return 0;
    }
    return 1;
}
