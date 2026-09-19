#pragma once

#include <cstdint>

// xoshiro256** (Blackman & Vigna, 2018; public domain reference algorithm,
// https://prng.di.unimi.it/xoshiro256starstar.c), reproduced here as a
// small stateful class rather than re-derived, since this is a widely
// reviewed, standard PRNG and re-deriving the bit-mixing constants from
// scratch would risk introducing a subtle statistical defect.
//
// Chosen per spec ("RNG (PCG or xoshiro) is explicit, seeded, and stored
// in SimState") for a tiny (32-byte) state that fits cleanly into a POD
// SimState, and because it's fast enough to call freely inside the sim
// step without a second thought.
namespace sim {

struct RngState {
    std::uint64_t s[4] = {0, 0, 0, 0};
};

class Rng {
public:
    explicit Rng(RngState* state) : state_(state) {}

    // xoshiro256** must never be seeded with an all-zero state (it's a
    // fixed point), and seeding its 4 words directly from small/related
    // values gives poor early output. SplitMix64 (also Vigna) is the
    // standard way to expand one 64-bit seed into 4 well-mixed words.
    static void seed(RngState* state, std::uint64_t seed_value) {
        std::uint64_t z = seed_value;
        for (int i = 0; i < 4; ++i) {
            z += 0x9E3779B97F4A7C15ull;
            std::uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            x = x ^ (x >> 31);
            state->s[i] = x;
        }
    }

    std::uint64_t next_u64() {
        std::uint64_t* s = state_->s;
        const std::uint64_t result = rotl(s[1] * 5, 7) * 9;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    // Uniform double in [0, 1), using the top 53 bits (exact double
    // mantissa precision).
    double next_double() { return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0); }

    // Uniform double in [lo, hi).
    double next_range(double lo, double hi) { return lo + next_double() * (hi - lo); }

private:
    static std::uint64_t rotl(std::uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

    RngState* state_;
};

}  // namespace sim
