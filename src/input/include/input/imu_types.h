#pragma once

#include <cstdint>

#include "common/ring_buffer.h"

// Raw (pre-filter) sensor sample types. These are what an ImuSource
// produces; bias calibration, clip recovery, and the orientation filter
// all consume them, regardless of whether they came from a real Joy-Con or
// a synthetic mouse/keyboard dev source -- "these exercise the same input
// path" per the spec.
namespace input {

struct ImuSample {
    std::uint64_t timestamp_ns = 0;  // device/source domain, NOT host time
    float gyro[3] = {0, 0, 0};       // rad/s, raw (bias not yet subtracted)
    float accel[3] = {0, 0, 0};      // m/s^2, raw
    // Bit i set if axis i saturated the device profile's documented range.
    // Bits 0-2: gyro x,y,z. Bits 3-5: accel x,y,z.
    std::uint8_t clip_flags = 0;
};

struct ButtonEvent {
    std::uint64_t timestamp_ns = 0;
    std::uint16_t button = 0;  // source-defined id; binding table maps this later
    bool down = false;
};

// ~1 s of history at the documented Joy-Con nominal rate (a couple hundred
// Hz), rounded up to a power of two per the ring buffer's requirement.
// Buttons are far less frequent, so a much smaller ring suffices.
inline constexpr std::size_t kImuRingCapacity = 512;
inline constexpr std::size_t kButtonRingCapacity = 64;

using ImuRing = common::RingBuffer<ImuSample, kImuRingCapacity>;
using ButtonRing = common::RingBuffer<ButtonEvent, kButtonRingCapacity>;

}  // namespace input
