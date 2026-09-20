#pragma once

#include <cstdint>

#include "input/binding_table.h"

// Milestone 7 role swap: "the roles swap between human players on a
// configurable schedule ... The swap is a sim command that changes role
// bindings at a specified tick, and player calibration follows the
// player, not the role."
//
// Deliberately kept OUT of SimState: the sim only ever sees role-keyed
// PlayerInput (sim::SimInputs::pitcher / ::batter) and has no concept of
// player slots at all -- "the sim only ever sees PlayerInput, it cannot
// tell the sources apart." Which physical slot currently feeds which
// role is therefore a pure input-binding question, decided identically
// by whichever process is driving the sim (today: the single local
// process; later: every peer in an online session, from the same
// deterministic schedule + completed-at-bat count, with no extra message
// needed). This mirrors how milestone 6 kept at-bat orchestration state
// out of SimState and in the driving loop instead.
namespace versus {

enum class Role : std::uint8_t { kPitcher = 0, kBatter = 1 };

enum class SwapMode : std::uint8_t {
    kManual,          // "same roles all game" / explicit swap only -- spec's sandbox-testing option
    kEveryNAtBats,
    kEveryHalfInning,
};

struct RoleScheduleConfig {
    SwapMode mode = SwapMode::kManual;
    std::uint32_t n_at_bats = 3;  // used only when mode == kEveryNAtBats
};

// Which PlayerSlot currently holds which role. Always a full two-way
// swap (pitcher<->batter) since there are exactly two roles and two
// slots -- never a partial reassignment.
struct RoleAssignment {
    input::PlayerSlot pitcher_slot = input::PlayerSlot::kP1;
    input::PlayerSlot batter_slot = input::PlayerSlot::kP2;

    input::PlayerSlot slot_for(Role role) const { return role == Role::kPitcher ? pitcher_slot : batter_slot; }
    Role role_for(input::PlayerSlot slot) const { return slot == pitcher_slot ? Role::kPitcher : Role::kBatter; }

    void swap() {
        const input::PlayerSlot tmp = pitcher_slot;
        pitcher_slot = batter_slot;
        batter_slot = tmp;
    }
};

// Pure function: given the schedule and how many at-bats have completed
// since the last swap, should a swap happen now? The caller applies
// RoleAssignment::swap() at whatever tick boundary this becomes true --
// deterministic given schedule config + completed-at-bat count, both of
// which are already known identically wherever the sim is being driven,
// with no wall-clock or device state involved.
inline bool should_swap_role(const RoleScheduleConfig& config, std::uint32_t completed_at_bats_since_last_swap) {
    switch (config.mode) {
        case SwapMode::kManual:
            return false;  // caller drives swaps explicitly (e.g. a menu action)
        case SwapMode::kEveryNAtBats:
            return config.n_at_bats > 0 && completed_at_bats_since_last_swap >= config.n_at_bats;
        case SwapMode::kEveryHalfInning:
            // Sandbox scope (per docs/ORIGINAL_PROMPT.md's "at-bat loop
            // (sandbox scope)") has no innings/outs game-loop yet -- that's
            // beyond this milestone's at-bat classification work. Treat a
            // half-inning as 3 at-bats (3 outs) as a documented placeholder
            // until a real innings model exists.
            return completed_at_bats_since_last_swap >= 3;
    }
    return false;
}

}  // namespace versus
