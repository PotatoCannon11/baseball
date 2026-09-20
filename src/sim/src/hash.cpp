#include "sim/hash.h"

#include <cstring>

namespace sim {

namespace {

// FNV-1a, 64-bit.
constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

void fold_u64(std::uint64_t* hash, std::uint64_t value) {
    // Fold 8 bytes at a time rather than byte-by-byte: same FNV-1a
    // mixing, fewer iterations. Equivalent to feeding the bytes in
    // little-endian order through the standard byte-at-a-time algorithm.
    for (int i = 0; i < 8; ++i) {
        const std::uint8_t byte = static_cast<std::uint8_t>(value >> (8 * i));
        *hash ^= byte;
        *hash *= kFnvPrime;
    }
}

void fold_double(std::uint64_t* hash, double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    fold_u64(hash, bits);
}

void fold_vec3(std::uint64_t* hash, const Vec3& v) {
    fold_double(hash, v.x);
    fold_double(hash, v.y);
    fold_double(hash, v.z);
}

void fold_quat(std::uint64_t* hash, const Quat& q) {
    fold_double(hash, q.w);
    fold_double(hash, q.x);
    fold_double(hash, q.y);
    fold_double(hash, q.z);
}

}  // namespace

std::uint64_t hash_state(const SimState& state) {
    std::uint64_t hash = kFnvOffsetBasis;

    fold_u64(&hash, state.tick);

    fold_vec3(&hash, state.ball.position);
    fold_vec3(&hash, state.ball.velocity);
    fold_quat(&hash, state.ball.orientation);
    fold_vec3(&hash, state.ball.angular_velocity);

    fold_vec3(&hash, state.bat.pivot_position);
    fold_quat(&hash, state.bat.orientation);
    fold_vec3(&hash, state.bat.angular_velocity);

    fold_vec3(&hash, state.pitcher_arm.pivot_position);
    fold_quat(&hash, state.pitcher_arm.orientation);
    fold_vec3(&hash, state.pitcher_arm.angular_velocity);
    fold_u64(&hash, state.pitcher_arm.prev_buttons);

    fold_u64(&hash, state.ready.pitcher_ready ? 1 : 0);
    fold_u64(&hash, state.ready.batter_ready ? 1 : 0);
    fold_u64(&hash, state.ready.wait_start_tick);

    for (std::uint64_t word : state.rng.s) fold_u64(&hash, word);

    return hash;
}

}  // namespace sim
