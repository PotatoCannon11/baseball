#include "input/sdl_input_hub.h"

#include <cstdio>

namespace input {

JoyconSource* SdlInputHub::find_joycon(SDL_JoystickID id) {
    for (auto& slot : joycons_) {
        if (slot.has_value() && slot->id() == id) return &*slot;
    }
    return nullptr;
}

void SdlInputHub::handle_device_added(SDL_JoystickID id) {
    if (find_joycon(id)) return;  // already tracked; shouldn't happen, but be safe
    for (auto& slot : joycons_) {
        if (!slot.has_value()) {
            SDL_Gamepad* gp = SDL_OpenGamepad(id);
            if (gp) slot.emplace(gp);
            return;
        }
    }
    // No free tracking slot (kMaxJoyconSources reached): this extra device
    // is ignored until one of the tracked ones disconnects. Not fatal.
}

void SdlInputHub::handle_device_removed(SDL_JoystickID id) {
    binding_table_.release_device(id);
    for (auto& slot : joycons_) {
        if (slot.has_value() && slot->id() == id) {
            slot.reset();
            return;
        }
    }
}

void SdlInputHub::poll(float dt_seconds) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                quit_requested_ = true;
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                handle_device_added(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                handle_device_removed(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
                if (JoyconSource* js = find_joycon(event.gsensor.which)) js->ingest(event);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (JoyconSource* js = find_joycon(event.gbutton.which)) js->ingest(event);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (debug_logging_) {
                    std::fprintf(stderr, "[debug] SDL_EVENT_MOUSE_MOTION x=%.1f y=%.1f xrel=%.1f yrel=%.1f\n",
                                 event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                }
                mouse_source_.ingest(event);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (debug_logging_) {
                    std::fprintf(stderr, "[debug] SDL_EVENT_MOUSE_BUTTON_%s button=%u x=%.1f y=%.1f\n",
                                 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "DOWN" : "UP", event.button.button,
                                 event.button.x, event.button.y);
                }
                mouse_source_.ingest(event);
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (debug_logging_) {
                    std::fprintf(stderr, "[debug] SDL_EVENT_KEY_%s key=%u repeat=%d\n",
                                 event.type == SDL_EVENT_KEY_DOWN ? "DOWN" : "UP",
                                 static_cast<unsigned>(event.key.key), event.key.repeat);
                }
                keyboard_source_.ingest(event);
                break;
            case SDL_EVENT_WINDOW_MOUSE_ENTER:
                if (debug_logging_) std::fprintf(stderr, "[debug] SDL_EVENT_WINDOW_MOUSE_ENTER\n");
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                if (debug_logging_) std::fprintf(stderr, "[debug] SDL_EVENT_WINDOW_FOCUS_GAINED\n");
                break;
            default:
                break;
        }
    }

    mouse_source_.pump_with_dt(dt_seconds);
    keyboard_source_.pump_with_dt(dt_seconds);
}

bool SdlInputHub::claim_slot(PlayerSlot slot, SDL_JoystickID device_id) {
    if (!find_joycon(device_id)) return false;
    return binding_table_.claim(slot, device_id);
}

void SdlInputHub::release_slot(PlayerSlot slot) { binding_table_.release(slot); }

JoyconSource* SdlInputHub::joycon_for_slot(PlayerSlot slot) {
    const BindingTable::DeviceId device_id = binding_table_.device_for_slot(slot);
    if (device_id == BindingTable::kNoDevice) return nullptr;
    JoyconSource* js = find_joycon(device_id);
    return (js && js->is_connected()) ? js : nullptr;
}

SDL_JoystickID SdlInputHub::first_unclaimed_connected_device() const {
    for (const auto& slot : joycons_) {
        if (!slot.has_value() || !slot->is_connected()) continue;
        PlayerSlot bound_slot;
        if (!binding_table_.slot_for_device(slot->id(), &bound_slot)) return slot->id();
    }
    return BindingTable::kNoDevice;
}

std::size_t SdlInputHub::connected_joycon_count() const {
    std::size_t n = 0;
    for (const auto& slot : joycons_) {
        if (slot.has_value() && slot->is_connected()) ++n;
    }
    return n;
}

}  // namespace input
