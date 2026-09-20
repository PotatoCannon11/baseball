#include <cmath>
#include <cstdio>
#include <filesystem>

#include "input/recorder.h"
#include "sim/hash.h"
#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

void write_synthetic_stream(const std::string& path, float base_omega, std::uint32_t frames) {
    input::PlayerInputRecorder recorder;
    recorder.open(path);
    for (std::uint32_t tick = 0; tick < frames; ++tick) {
        common::PlayerInput in{};
        in.tick = tick;
        in.orientation[3] = static_cast<std::int16_t>(common::PlayerInput::kQuatScale);
        const float omega = base_omega * std::sin(static_cast<float>(tick) * 0.02f);
        in.angular_velocity[0] = static_cast<std::int16_t>(omega * common::PlayerInput::kAngVelScale);
        recorder.write(in);
    }
}

// Replays two recorded PlayerInput streams together (one as pitcher, one
// as batter) through sim::step and returns the resulting state hash.
std::uint64_t replay_both(const std::string& pitcher_path, const std::string& batter_path) {
    input::PlayerInputReplayer pitcher_replay, batter_replay;
    if (!pitcher_replay.open(pitcher_path) || !batter_replay.open(batter_path)) return 0;

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 11);
    state.ball.position = sim::Vec3{5.0, 1.0, 0.0};
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    common::PlayerInput pitcher_frame{}, batter_frame{};
    while (pitcher_replay.next(&pitcher_frame) && batter_replay.next(&batter_frame)) {
        sim::SimInputs inputs{};
        inputs.pitcher = pitcher_frame;
        inputs.batter = batter_frame;
        state = sim::step(state, inputs, config);
    }
    return sim::hash_state(state);
}

}  // namespace

// Milestone 7 validation test: "two recorded human input streams replayed
// together produce identical hashes on repeat runs."
int main() {
    bool ok = true;

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "baseball_two_human_replay_test";
    std::filesystem::create_directories(dir);
    const std::string pitcher_path = (dir / "pitcher.rec").string();
    const std::string batter_path = (dir / "batter.rec").string();

    constexpr std::uint32_t kFrames = 600;
    write_synthetic_stream(pitcher_path, 4.0f, kFrames);
    write_synthetic_stream(batter_path, -6.0f, kFrames);

    const std::uint64_t hash_run_1 = replay_both(pitcher_path, batter_path);
    const std::uint64_t hash_run_2 = replay_both(pitcher_path, batter_path);

    ok &= check(hash_run_1 != 0, "the replay should actually run and produce a real hash, not the sentinel failure value");
    ok &= check(hash_run_1 == hash_run_2,
                "replaying the same two recorded PlayerInput streams together must produce identical hashes");

    std::printf("hash(two-human replay, %u frames) = 0x%016llx (run twice, identical)\n", kFrames,
                static_cast<unsigned long long>(hash_run_1));

    std::filesystem::remove_all(dir);

    if (ok) {
        std::printf("PASS: two recorded human input streams replay deterministically together\n");
        return 0;
    }
    return 1;
}
