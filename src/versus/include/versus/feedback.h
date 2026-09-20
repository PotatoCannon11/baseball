#pragma once

#include <cstdint>

#include "input/binding_table.h"
#include "input/joycon_source.h"

// Milestone 7 private feedback: "The shared screen must not leak private
// decisions ... Private feedback goes through the controller instead: a
// distinct rumble pattern per selected grip, and a player-LED or short
// haptic confirmation, emitted as private output events (player-targeted,
// visibility flag) and played only if the platform and driver support
// it. Report what SDL3 supports on each OS."
namespace versus {

enum class PrivateFeedbackKind : std::uint8_t {
    kGripRumble,         // distinct rumble pattern identifying the selected grip
    kReadyConfirmation,  // short haptic/LED confirmation of a ready-up or recenter
};

// player-targeted (never broadcast): only PrivateFeedbackEvent::target's
// controller is touched.
struct PrivateFeedbackEvent {
    input::PlayerSlot target;
    PrivateFeedbackKind kind;
    std::uint8_t grip_index = 0;  // meaningful only for kGripRumble; see grip_rumble_pattern()
};

// One rumble pattern per grip preset (4 presets, matching
// sim::GripTable), distinguishable by feel alone: increasing motor split
// and duration so preset 0 is a short uniform buzz and preset 3 is a
// longer, lopsided one. Arbitrary but fixed and documented, same spirit
// as MouseKeyboardSource's drag-axis mapping -- not claimed to be
// "correct," just a consistent, testable signal.
struct RumblePattern {
    std::uint16_t low_frequency;
    std::uint16_t high_frequency;
    std::uint32_t duration_ms;
};

inline RumblePattern grip_rumble_pattern(std::uint8_t grip_index) {
    switch (grip_index % 4) {
        case 0:
            return RumblePattern{0x4000, 0x4000, 80};
        case 1:
            return RumblePattern{0x8000, 0x2000, 120};
        case 2:
            return RumblePattern{0x2000, 0x8000, 160};
        default:
            return RumblePattern{0xC000, 0xC000, 200};
    }
}

// Plays event on gamepad if the target slot's event and SDL/driver both
// support it; returns whether SDL reported the call as honored (SDL3 has
// no separate capability-query API for rumble or LED -- the return value
// of the call itself is the only signal available). UNVERIFIED on real
// hardware, same caveat as JoyconSource::rumble()/set_led().
bool play_private_feedback(input::JoyconSource* gamepad, const PrivateFeedbackEvent& event);

}  // namespace versus
