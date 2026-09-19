#pragma once

#include <SDL3/SDL.h>

#include <optional>

#include "input/binding_table.h"
#include "input/joycon_source.h"
#include "input/mouse_keyboard_source.h"

// Owns the single SDL_PollEvent loop for the whole app. SDL's event queue
// is process-global -- only one place may drain it -- so every SDL-fed
// source (Joy-Con gamepads, the mouse/keyboard dev sources) gets routed
// through here instead of polling independently.
//
// Also owns the multi-device claim/binding table: "Keep the slot-to-device
// binding in a small fixed table in the input layer, never in the sim."
// Gamepads are opened automatically when SDL reports them added, but start
// unclaimed; something upstream (a "press a button to join" flow, or a
// tool/test) must call claim_slot() to bind one to a player slot.
namespace input {

class SdlInputHub {
public:
    static constexpr std::size_t kMaxJoyconSources = 4;

    // Drains all pending SDL events once, dispatches them to the matching
    // source, and pumps every self-pumping source. Call once per frame.
    void poll(float dt_seconds);

    // When enabled, prints every mouse/keyboard/gamepad event this hub
    // sees to stderr as it's dispatched. Diagnostic only, off by default:
    // meant for tracking down "is SDL even delivering events to this
    // window" questions that are otherwise hard to debug remotely.
    void set_debug_logging(bool enabled) { debug_logging_ = enabled; }

    bool claim_slot(PlayerSlot slot, SDL_JoystickID device_id);
    void release_slot(PlayerSlot slot);

    // First connected Joy-Con not currently bound to any slot, or
    // BindingTable::kNoDevice if none. Convenience for tools/tests that
    // want to auto-claim "whatever's plugged in" without a real join flow.
    SDL_JoystickID first_unclaimed_connected_device() const;

    // Returns null if the slot isn't bound to a (still connected) Joy-Con.
    JoyconSource* joycon_for_slot(PlayerSlot slot);

    const BindingTable& binding_table() const { return binding_table_; }
    std::size_t connected_joycon_count() const;

    MouseKeyboardSource& mouse_drag_source() { return mouse_source_; }
    MouseKeyboardSource& keyboard_source() { return keyboard_source_; }

private:
    JoyconSource* find_joycon(SDL_JoystickID id);
    void handle_device_added(SDL_JoystickID id);
    void handle_device_removed(SDL_JoystickID id);

    // Fixed-capacity slots for Joy-Con sources. std::optional so a slot can
    // be empty without heap allocation or needing JoyconSource to be
    // default-constructible; emplace()/reset() construct/destroy in place
    // and never copy or move the contained JoyconSource.
    std::optional<JoyconSource> joycons_[kMaxJoyconSources];

    BindingTable binding_table_;
    MouseKeyboardSource mouse_source_{MouseKeyboardSource::Mode::kMouseDrag};
    MouseKeyboardSource keyboard_source_{MouseKeyboardSource::Mode::kKeyboard};
    bool debug_logging_ = false;
};

}  // namespace input
