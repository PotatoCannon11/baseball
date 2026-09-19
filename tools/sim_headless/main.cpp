// Milestone 3 headless executable: runs the sim for a fixed number of
// ticks with no window, no GL, and prints the final state hash -- this is
// literally "a tool that prints a state hash after a scripted run so I can
// compare across platforms" from the determinism section of the spec.
// Also reports step() cost (--bench) to justify the chosen tick rate, and
// allocation counts during the run (the sim/frame loop must allocate
// zero times after startup).

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "alloc/alloc_counter.h"
#include "platform/clock.h"
#include "sim/hash.h"
#include "sim/step.h"

namespace {

struct Args {
    std::uint32_t seed = 42;
    std::uint32_t ticks = 2400;  // 10 s at the default 240 Hz tick rate
    bool bench = false;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            a.seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            a.ticks = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--bench") == 0) {
            a.bench = true;
        }
    }
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, args.seed);
    state.ball.position = sim::Vec3{1.5, 1.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();

    // Milestone 3 has no real bat/arm interaction yet (that's milestones
    // 4/5), so a fixed zero PlayerInput is a legitimate "scripted run":
    // deterministic, reproducible, and exercises the exact same step()
    // path a live game would use.
    const sim::SimInputs inputs{};

    const alloc::AllocStats before = alloc::get_stats();
    const std::uint64_t bench_start_ns = args.bench ? platform::monotonic_now_ns() : 0;

    for (std::uint32_t i = 0; i < args.ticks; ++i) {
        state = sim::step(state, inputs, config);
    }

    const std::uint64_t bench_elapsed_ns =
        args.bench ? (platform::monotonic_now_ns() - bench_start_ns) : 0;
    const alloc::AllocStats after = alloc::get_stats();

    std::printf("seed=%u ticks=%u final_tick=%u hash=0x%016llx\n", args.seed, args.ticks, state.tick,
                static_cast<unsigned long long>(sim::hash_state(state)));
    std::printf("ball: pos=(%.4f, %.4f, %.4f) vel=(%.4f, %.4f, %.4f)\n", state.ball.position.x,
                state.ball.position.y, state.ball.position.z, state.ball.velocity.x, state.ball.velocity.y,
                state.ball.velocity.z);

    const std::uint64_t allocations_during_run = after.total_allocations - before.total_allocations;
    std::printf("allocations during run: %llu (must be 0)\n",
                static_cast<unsigned long long>(allocations_during_run));

    if (args.bench && args.ticks > 0) {
        std::printf("step() cost: %.1f ns/tick average over %u ticks (%.3f ms total)\n",
                    static_cast<double>(bench_elapsed_ns) / args.ticks, args.ticks, bench_elapsed_ns / 1e6);
        std::printf("at 240 Hz, tick budget is %.1f us; measured cost leaves %.2f%% headroom\n", 1e6 / 240.0,
                    100.0 * (1.0 - (static_cast<double>(bench_elapsed_ns) / args.ticks) / (1e9 / 240.0)));
    }

    return allocations_during_run == 0 ? 0 : 1;
}
