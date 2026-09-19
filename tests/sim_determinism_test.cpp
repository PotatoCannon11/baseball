#include <cstdio>

#include "sim/hash.h"
#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

sim::SimState run(std::uint32_t seed, std::uint32_t ticks) {
    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, seed);
    state.ball.position = sim::Vec3{1.5, 1.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();

    const sim::SimInputs inputs{};
    for (std::uint32_t i = 0; i < ticks; ++i) {
        state = sim::step(state, inputs, config);
    }
    return state;
}

}  // namespace

// "Same binary, same inputs, same outputs, bit for bit (test required)."
int main() {
    bool ok = true;

    const sim::SimState a = run(42, 1000);
    const sim::SimState b = run(42, 1000);

    ok &= check(a.tick == b.tick, "two runs with identical seed/inputs should reach the same tick");
    ok &= check(sim::hash_state(a) == sim::hash_state(b),
                "two runs with identical seed/inputs must hash identically, bit for bit");

    // Different seeds should (with overwhelming probability) NOT produce
    // an identical hash -- a sanity check that the hash isn't trivially
    // constant or that the RNG seed isn't silently ignored. Milestone 3's
    // step() doesn't consume randomness yet, so this mainly checks the
    // rng field itself is part of the hash.
    const sim::SimState c = run(43, 1000);
    ok &= check(sim::hash_state(a) != sim::hash_state(c),
                "a different seed should change the hash (rng state is part of what's hashed)");

    std::printf("hash(seed=42, 1000 ticks) = 0x%016llx (run twice, identical)\n",
                static_cast<unsigned long long>(sim::hash_state(a)));

    if (ok) {
        std::printf("PASS: sim determinism holds across repeated runs\n");
        return 0;
    }
    return 1;
}
