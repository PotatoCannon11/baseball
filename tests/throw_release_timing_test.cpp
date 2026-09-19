#include <cstdio>

#include "sim/step.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// "Release is a shoulder-trigger button-up event, looked up at its
// timestamp." Holding the throw button should NOT release the ball; only
// the tick where it transitions from held to not-held should.
int main() {
    bool ok = true;

    sim::SimConfig config;
    sim::SimState state{};
    sim::Rng::seed(&state.rng, 1);
    state.ball.position = sim::Vec3{5.0, 1.0, 0.0};  // parked away from the bat, not part of this test
    state.ball.orientation = sim::Quat::identity();
    state.bat.orientation = sim::Quat::identity();
    state.pitcher_arm.orientation = sim::Quat::identity();

    common::PlayerInput pitcher{};
    pitcher.angular_velocity[0] =
        static_cast<std::int16_t>(20.0f * common::PlayerInput::kAngVelScale);  // some arm motion throughout
    pitcher.buttons = static_cast<std::uint16_t>(common::InputButton::kThrow);

    sim::SimInputs inputs{};
    inputs.pitcher = pitcher;

    // Hold the throw button for several ticks -- no release should occur.
    // The ball is just falling under gravity/ground physics this whole
    // time; the actual thing under test is that it never jumps to the
    // arm's release point.
    const sim::ArmProperties arm_props;  // default virtual_arm_length_m
    const sim::Vec3 release_point{0.0, 1.8 + arm_props.virtual_arm_length_m, 18.0};  // arm pivot (0,1.8,18) + lever
    for (int i = 0; i < 5; ++i) {
        state = sim::step(state, inputs, config);
        ok &= check((state.ball.position - release_point).length() > 1.0,
                    "holding the throw button should not release the ball");
    }

    // Release: button goes from held to not-held this tick.
    inputs.pitcher.buttons = 0;
    const std::uint32_t tick_before_release = state.tick;
    state = sim::step(state, inputs, config);

    ok &= check(state.tick == tick_before_release + 1, "step should still advance exactly one tick");
    ok &= check((state.ball.position - release_point).length() < 1e-6,
                "the ball should launch from the arm's release point on the button-up tick");
    ok &= check(state.ball.velocity.length() > 1.0, "the released ball should have nonzero velocity");
    const sim::Vec3 velocity_at_release = state.ball.velocity;

    // The following tick, with the button still not held, should NOT
    // re-release (no repeated launches from a single button-up edge) --
    // the ball should just continue flying normally, a small continuous
    // step away from the release point rather than being reset back onto
    // it.
    state = sim::step(state, inputs, config);
    const double moved = (state.ball.position - release_point).length();
    ok &= check(moved > 0.01 && moved < 1.0,
                "a second tick with the button already up should continue normal flight, not re-launch");
    ok &= check((state.ball.velocity - velocity_at_release).length() < 5.0,
                "velocity should evolve continuously (aerodynamics only), not jump back to a fresh launch");

    if (ok) {
        std::printf("PASS: release fires exactly on the throw button's up-edge, once\n");
        return 0;
    }
    return 1;
}
