#pragma once

#include <cstdint>

#include "sim/state.h"

// Deterministic state hash, for desync detection and the determinism
// tests. Hashes explicit named fields one at a time (not a raw memcpy of
// SimState) so compiler-inserted struct padding -- only guaranteed zeroed
// by value-initialization, easy to get wrong at some future call site --
// can never leak into the hash and cause a spurious mismatch between two
// logically-identical states.
//
// Doubles are hashed via their exact bit pattern: no rounding, no
// tolerance. That's what "same binary, same inputs, same outputs, bit for
// bit" requires. This does NOT promise the hash matches across platforms
// or compilers (see sim/math.h) -- only across runs of the same binary.
namespace sim {

std::uint64_t hash_state(const SimState& state);

}  // namespace sim
