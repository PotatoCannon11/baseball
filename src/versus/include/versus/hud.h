#pragma once

#include <cstdint>

// Milestone 7 shared HUD: "Shared HUD only shows public information:
// count, score, outs, last pitch result, speed after release. Debug
// overlay is dev-only and hidden in play mode."
//
// This struct is the enforcement mechanism, not just a description of
// intent: it has no field for pitch type, grip, or aim target, so there
// is no member to accidentally draw -- a caller literally cannot read
// private information out of a PublicHud because it was never put in.
// (tests/versus_hud_privacy_test.cpp guards this file's text against a
// later field slipping in.)
namespace versus {

enum class LastPitchResult : std::uint8_t {
    kNone,  // no pitch resolved yet this at-bat/session
    kCalledBall,
    kCalledStrike,
    kSwingingStrike,
    kFoul,
    kFairBall,
};

struct PublicHud {
    std::uint8_t balls = 0;
    std::uint8_t strikes = 0;
    std::uint8_t outs = 0;
    std::uint16_t score_p1 = 0;
    std::uint16_t score_p2 = 0;

    LastPitchResult last_pitch_result = LastPitchResult::kNone;
    // "speed after release" -- valid only once a pitch has actually been
    // released this at-bat; the driver must not populate this before
    // release, and there is deliberately no pre-release field (e.g. a
    // target/aim speed) for it to be confused with.
    double last_pitch_speed_mps = 0.0;
    bool last_pitch_speed_valid = false;
};

}  // namespace versus
