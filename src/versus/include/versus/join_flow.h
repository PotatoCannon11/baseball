#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "input/binding_table.h"
#include "sim/step.h"

// Milestone 7 join/disconnect flow: "each controller is claimed by a
// player slot ... Hot-plug and disconnect: if a bound controller drops
// during play, pause deterministically at the next tick boundary, show a
// reconnect prompt, and resume on reconnect or let the player switch to a
// CPU. Never crash and never let a missing device inject zeros into a
// live pitch."
//
// JoinFlow itself is a plain state machine over explicit transitions
// (mark_claimed/mark_disconnected/mark_reconnected/release) so it is
// fully unit-testable without SDL or a real/virtual device -- a
// "simulated controller disconnect" is exactly a direct call to
// mark_disconnected(). Watching a real SdlInputHub and calling those
// transitions is a separate, thin piece of glue (observe_hub below) kept
// out of this class per the same "pure logic vs. SDL glue" split used
// throughout the input layer (e.g. BindingTable vs. SdlInputHub).
namespace input {
class SdlInputHub;
}  // namespace input

namespace versus {

enum class SlotState : std::uint8_t {
    kUnclaimed,     // no controller bound yet -- "press a button to join"
    kActive,        // bound and its device is connected
    kDisconnected,  // was bound; device dropped mid-play -- paused, awaiting reconnect or a switch to CPU
};

class JoinFlow {
public:
    void mark_claimed(input::PlayerSlot slot) { slots_[index(slot)] = SlotState::kActive; }
    void release(input::PlayerSlot slot) { slots_[index(slot)] = SlotState::kUnclaimed; }

    // Only meaningful from kActive; a slot that was never claimed has no
    // device to lose.
    void mark_disconnected(input::PlayerSlot slot) {
        if (slots_[index(slot)] == SlotState::kActive) slots_[index(slot)] = SlotState::kDisconnected;
    }
    // Only meaningful from kDisconnected.
    void mark_reconnected(input::PlayerSlot slot) {
        if (slots_[index(slot)] == SlotState::kDisconnected) slots_[index(slot)] = SlotState::kActive;
    }

    SlotState state(input::PlayerSlot slot) const { return slots_[index(slot)]; }

    // True if any bound slot currently has a dropped device -- the app
    // loop's signal to stop calling sim::step() and hold at the current
    // tick until every disconnected slot either reconnects or is
    // released (switched to CPU).
    bool should_pause() const {
        for (SlotState s : slots_) {
            if (s == SlotState::kDisconnected) return true;
        }
        return false;
    }

private:
    static std::size_t index(input::PlayerSlot slot) { return static_cast<std::size_t>(slot); }
    std::array<SlotState, input::kMaxPlayerSlots> slots_{SlotState::kUnclaimed, SlotState::kUnclaimed};
};

// Observes a live SdlInputHub and applies disconnect/reconnect
// transitions to flow for both player slots. Call once per frame, after
// hub.poll(). UNVERIFIED against a real disconnect event (this project
// has had no real Joy-Con to physically unplug); JoinFlow's own
// transition logic above is what the tests actually exercise.
// Takes hub by non-const reference: SdlInputHub::joycon_for_slot() isn't
// const (it walks a fixed array of std::optional<JoyconSource> to find a
// match, which the current implementation doesn't mark const), and this
// is only ever called from the same frame loop that already owns a
// mutable hub, so there's no reason to fight that here.
void observe_hub(JoinFlow* flow, input::SdlInputHub& hub);

// "Pause deterministically at the next tick boundary ... never let a
// missing device inject zeros into a live pitch": if flow.should_pause()
// is true, returns prev completely unchanged (the tick counter does not
// advance, no input is synthesized for the missing player) instead of
// calling sim::step(). Resuming is simply calling this again once
// should_pause() is false -- the sim never even knows a pause happened.
sim::SimState step_or_pause(const sim::SimState& prev, const sim::SimInputs& inputs, const sim::SimConfig& config,
                             const JoinFlow& flow, sim::StepEvents* events = nullptr);

}  // namespace versus
