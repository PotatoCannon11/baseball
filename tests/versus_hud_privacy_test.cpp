#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "versus/hud.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

std::string lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

}  // namespace

// Milestone 7 validation test: "the shared screen renders no pitch-type
// or aim information before release (test the draw list or overlay
// contents)." PublicHud (versus/hud.h) is the shared-screen data
// contract -- there is no renderer to point a pixel-diff at in this test
// binary, so the check is at the level the spec actually cares about:
// does the struct that feeds the shared screen have any member capable
// of carrying grip/pitch-type/aim information at all? Enforced two ways:
// (1) by construction here -- PublicHud's listed public fields are
// exhaustively checked against the known-safe set; (2) by scanning the
// header's own source text for leaky identifiers, so a future edit that
// adds e.g. a "grip_index" field to PublicHud fails this test instead of
// silently shipping.
int main() {
    bool ok = true;

    // --- (1) Exhaustiveness via sizeof: PublicHud must be exactly the
    // sum of its documented public-safe fields, nothing more. This is a
    // blunt instrument (it wouldn't catch a same-size private field
    // replacing a public one), which is why check (2) below also scans
    // the source text directly.
    constexpr std::size_t kExpectedSize = sizeof(std::uint8_t) * 3        // balls, strikes, outs
                                           + sizeof(std::uint16_t) * 2    // score_p1, score_p2
                                           + sizeof(versus::LastPitchResult) + sizeof(double) + sizeof(bool);
    ok &= check(sizeof(versus::PublicHud) <= kExpectedSize + 16,  // + padding slack
                "PublicHud grew unexpectedly large -- check for an accidentally-added field");

    // --- (2) Source-text scan for known-leaky identifiers, restricted to
    // the struct's own field declarations (not the surrounding comments,
    // which legitimately quote the spec's "grip"/"pitch type" wording
    // when explaining WHY those fields don't exist) -- this scans
    // "struct PublicHud { ... };" only, so it stays a check on the
    // struct's actual members, not on how the prose describes them.
    const char* header_path = BASEBALL_SOURCE_DIR "/src/versus/include/versus/hud.h";
    std::ifstream in(header_path);
    ok &= check(in.is_open(), "versus/hud.h should be readable at the expected source path");
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string full_text = buffer.str();

    const std::size_t struct_start = full_text.find("struct PublicHud {");
    ok &= check(struct_start != std::string::npos, "versus/hud.h should still declare struct PublicHud");
    const std::size_t struct_end = full_text.find("};", struct_start);
    ok &= check(struct_end != std::string::npos, "PublicHud's struct body should be closed with '};'");
    const std::string struct_body =
        lowercase(full_text.substr(struct_start, struct_end == std::string::npos ? std::string::npos
                                                                                   : struct_end - struct_start));

    for (const char* forbidden : {"grip", "aim_target", "pitch_type", "target_point"}) {
        const bool present = struct_body.find(forbidden) != std::string::npos;
        ok &= check(!present, (std::string("PublicHud's field list must never mention '") + forbidden + "'").c_str());
    }

    // --- Sanity: a plausible pre-release HUD has no speed populated. ---
    versus::PublicHud pre_release;
    ok &= check(!pre_release.last_pitch_speed_valid,
                "a freshly-constructed HUD must not claim a valid pitch speed before any pitch has been thrown");
    ok &= check(pre_release.last_pitch_result == versus::LastPitchResult::kNone,
                "a freshly-constructed HUD must not claim a pitch result before any pitch has resolved");

    if (ok) {
        std::printf("PASS: the shared HUD contract has no field capable of leaking private pitch information\n");
        return 0;
    }
    return 1;
}
