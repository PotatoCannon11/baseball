// Milestone 7 validation test: "memory with two humans is within the
// per-player input budget of memory with zero." Every per-player
// structure in this codebase (MotionPipeline, its ImuRing/ButtonRing,
// JoinFlow's fixed slot array) is a fixed-size, statically-sized member
// -- there is no per-connection heap allocation anywhere in this design
// -- so the strongest, most literal version of "within budget" holds:
// running the tick loop with zero simulated humans and with two produces
// IDENTICAL (zero) heap allocation deltas, not just "close."

#include <SDL3/SDL.h>

#include <cstdio>

#include "alloc/alloc_counter.h"
#include "input/motion_pipeline.h"
#include "input/mouse_keyboard_source.h"
#include "versus/join_flow.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

SDL_Event make_mouse_button_event(SDL_EventType type, Uint8 button, bool down, Uint64 timestamp_ns) {
    SDL_Event e{};
    e.type = type;
    e.button.type = type;
    e.button.timestamp = timestamp_ns;
    e.button.button = button;
    e.button.down = down;
    return e;
}

SDL_Event make_mouse_motion_event(float xrel, float yrel, Uint64 timestamp_ns) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.timestamp = timestamp_ns;
    e.motion.xrel = xrel;
    e.motion.yrel = yrel;
    return e;
}

// Drives one simulated "human": a mouse-drag source pushed through a
// MotionPipeline every tick, exactly like app/game's batter_pipeline.
std::uint64_t drive_one_human(int ticks) {
    input::MouseKeyboardSource source(input::MouseKeyboardSource::Mode::kMouseDrag);
    input::MotionPipeline pipeline;
    pipeline.start_bias_calibration();
    pipeline.finish_bias_calibration();

    source.ingest(make_mouse_button_event(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT, true, 0));

    std::uint64_t t_ns = 0;
    std::uint16_t sequence = 0;
    std::uint64_t consumed = 0;
    for (int i = 0; i < ticks; ++i) {
        t_ns += 4'000'000ull;  // ~240 Hz
        source.ingest(make_mouse_motion_event(2.0f, 1.0f, t_ns));
        consumed += pipeline.process(source.imu_samples());
        (void)pipeline.to_player_input(static_cast<std::uint32_t>(i), sequence++);
    }
    return consumed;
}

}  // namespace

int main() {
    SDL_Init(0);
    bool ok = true;

    constexpr int kTicks = 1000;

    // --- Zero humans: just the join-flow bookkeeping, no input sources. ---
    versus::JoinFlow flow_zero;
    const alloc::AllocStats before_zero = alloc::get_stats();
    for (int i = 0; i < kTicks; ++i) {
        (void)flow_zero.should_pause();
    }
    const alloc::AllocStats after_zero = alloc::get_stats();
    const std::uint64_t allocations_zero = after_zero.total_allocations - before_zero.total_allocations;

    // --- Two humans: join flow both slots claimed, plus a full
    // MotionPipeline per slot actually processing synthetic input every
    // tick (not just constructed and idle). ---
    versus::JoinFlow flow_two;
    flow_two.mark_claimed(input::PlayerSlot::kP1);
    flow_two.mark_claimed(input::PlayerSlot::kP2);

    const alloc::AllocStats before_two = alloc::get_stats();
    const std::uint64_t p1_consumed = drive_one_human(kTicks);
    const std::uint64_t p2_consumed = drive_one_human(kTicks);
    for (int i = 0; i < kTicks; ++i) {
        (void)flow_two.should_pause();
    }
    const alloc::AllocStats after_two = alloc::get_stats();
    const std::uint64_t allocations_two = after_two.total_allocations - before_two.total_allocations;

    std::printf("zero-human allocations over %d ticks: %llu\n", kTicks,
                static_cast<unsigned long long>(allocations_zero));
    std::printf("two-human allocations over %d ticks (p1 consumed=%llu, p2 consumed=%llu): %llu\n", kTicks,
                static_cast<unsigned long long>(p1_consumed), static_cast<unsigned long long>(p2_consumed),
                static_cast<unsigned long long>(allocations_two));

    ok &= check(allocations_zero == 0, "zero simulated humans should allocate nothing over the tick loop");
    ok &= check(allocations_two == 0,
                "two simulated humans, each fully processing synthetic input every tick, should also allocate "
                "nothing -- every per-player structure here is fixed-size, so the budget is not just 'bounded', "
                "it's identically zero");
    ok &= check(p1_consumed > 0 && p2_consumed > 0,
                "sanity: the two-human loop must have actually consumed real samples, not trivially no-opped");

    SDL_Quit();

    if (ok) {
        std::printf("PASS: memory with two simulated humans matches memory with zero (both allocate nothing)\n");
        return 0;
    }
    return 1;
}
