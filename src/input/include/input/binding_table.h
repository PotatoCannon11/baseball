#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Maps a physical device id to a player slot. Deliberately has no SDL
// dependency -- device ids are just opaque uint32s here (an SDL_JoystickID
// fits fine) -- so this claim/release logic is fully unit-testable without
// any hardware or even SDL initialized.
//
// "Keep the slot-to-device binding in a small fixed table in the input
// layer, never in the sim." Two slots for now (matches the local-versus
// two-human scope); extend kMaxPlayerSlots if that ever needs to grow.
namespace input {

enum class PlayerSlot : std::uint8_t { kP1 = 0, kP2 = 1 };
inline constexpr std::size_t kMaxPlayerSlots = 2;

class BindingTable {
public:
    using DeviceId = std::uint32_t;
    static constexpr DeviceId kNoDevice = 0;

    // Fails (returns false) if the device is already claimed by a
    // different slot -- callers must release() or release_device() first.
    bool claim(PlayerSlot slot, DeviceId device_id) {
        if (device_id == kNoDevice) return false;
        for (const auto& b : bindings_) {
            if (b.device_id == device_id && b.slot != slot) return false;
        }
        bindings_[static_cast<std::size_t>(slot)] = {slot, device_id};
        return true;
    }

    void release(PlayerSlot slot) {
        bindings_[static_cast<std::size_t>(slot)].device_id = kNoDevice;
    }

    // Used when a device disconnects: clears it from whichever slot (if
    // any) currently holds it.
    void release_device(DeviceId device_id) {
        for (auto& b : bindings_) {
            if (b.device_id == device_id) b.device_id = kNoDevice;
        }
    }

    DeviceId device_for_slot(PlayerSlot slot) const {
        return bindings_[static_cast<std::size_t>(slot)].device_id;
    }

    bool slot_for_device(DeviceId device_id, PlayerSlot* out_slot) const {
        if (device_id == kNoDevice) return false;
        for (const auto& b : bindings_) {
            if (b.device_id == device_id) {
                *out_slot = b.slot;
                return true;
            }
        }
        return false;
    }

    bool is_bound(PlayerSlot slot) const {
        return bindings_[static_cast<std::size_t>(slot)].device_id != kNoDevice;
    }

private:
    struct Binding {
        PlayerSlot slot;
        DeviceId device_id;
    };
    std::array<Binding, kMaxPlayerSlots> bindings_{
        Binding{PlayerSlot::kP1, kNoDevice},
        Binding{PlayerSlot::kP2, kNoDevice},
    };
};

}  // namespace input
