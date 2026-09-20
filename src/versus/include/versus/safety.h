#pragma once

// Milestone 7 safety: "on first launch and in the join flow, show a
// short reminder to use wrist straps, clear the space around each
// player, and keep distance between players, since two people are
// swinging in one room."
namespace versus {

inline constexpr const char* kSafetyReminder =
    "Two players are about to swing controllers in the same room.\n"
    "- Use the wrist strap.\n"
    "- Clear the space around you -- no people, pets, or breakables in swing range.\n"
    "- Keep distance between players so swings can't collide.";

}  // namespace versus
