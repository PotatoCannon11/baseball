#include <cstdio>

#include "alloc/alloc_counter.h"
#include "sim/step.h"

// "Add an allocation counter... and a test that fails if anything is
// allocated during a 60-second simulated run."
int main() {
    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 1);
    state.ball.position = sim::Vec3{1.5, 1.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    const sim::SimInputs inputs{};

    const std::uint32_t ticks_for_60s = static_cast<std::uint32_t>(60.0 / config.tick_dt_seconds);

    const alloc::AllocStats before = alloc::get_stats();
    for (std::uint32_t i = 0; i < ticks_for_60s; ++i) {
        state = sim::step(state, inputs, config);
    }
    const alloc::AllocStats after = alloc::get_stats();

    const std::uint64_t allocations = after.total_allocations - before.total_allocations;
    const std::uint64_t bytes = after.bytes_allocated - before.bytes_allocated;

    std::printf("60 s simulated (%u ticks): %llu allocations, %llu bytes\n", ticks_for_60s,
                static_cast<unsigned long long>(allocations), static_cast<unsigned long long>(bytes));

    if (allocations != 0) {
        std::fprintf(stderr, "FAIL: expected zero allocations during the sim loop, got %llu\n",
                     static_cast<unsigned long long>(allocations));
        return 1;
    }

    std::printf("PASS: zero allocations across a 60 s simulated run\n");
    return 0;
}
