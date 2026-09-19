#include "input/binding_table.h"

#include <cstdio>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

int main() {
    bool ok = true;
    using input::BindingTable;
    using input::PlayerSlot;

    BindingTable table;
    ok &= check(!table.is_bound(PlayerSlot::kP1), "P1 should start unbound");
    ok &= check(!table.is_bound(PlayerSlot::kP2), "P2 should start unbound");

    ok &= check(table.claim(PlayerSlot::kP1, 42), "claiming a free slot with a real device id should succeed");
    ok &= check(table.is_bound(PlayerSlot::kP1), "P1 should be bound after claim");
    ok &= check(table.device_for_slot(PlayerSlot::kP1) == 42, "P1 should map to device 42");

    PlayerSlot found_slot;
    ok &= check(table.slot_for_device(42, &found_slot) && found_slot == PlayerSlot::kP1,
                "reverse lookup should find P1 for device 42");

    // Same device can't be claimed by a different slot without releasing first.
    ok &= check(!table.claim(PlayerSlot::kP2, 42), "claiming an already-bound device to a different slot should fail");

    ok &= check(table.claim(PlayerSlot::kP2, 7), "claiming P2 with a different device should succeed");
    ok &= check(table.device_for_slot(PlayerSlot::kP1) == 42 && table.device_for_slot(PlayerSlot::kP2) == 7,
                "both slots should hold their distinct devices");

    // Re-claiming the SAME slot with the SAME device should be idempotent.
    ok &= check(table.claim(PlayerSlot::kP1, 42), "re-claiming the same slot+device should succeed");

    table.release_device(42);
    ok &= check(!table.is_bound(PlayerSlot::kP1), "P1 should be unbound after release_device(42)");
    ok &= check(table.is_bound(PlayerSlot::kP2), "P2 should be untouched by releasing device 42");

    table.release(PlayerSlot::kP2);
    ok &= check(!table.is_bound(PlayerSlot::kP2), "P2 should be unbound after release()");

    ok &= check(!table.claim(PlayerSlot::kP1, BindingTable::kNoDevice), "claiming kNoDevice should always fail");

    if (ok) {
        std::printf("PASS: binding table claim/release/lookup behave as expected\n");
        return 0;
    }
    return 1;
}
