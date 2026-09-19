#pragma once

#include "common/ring_buffer.h"
#include "sim/state.h"

// "A fixed-capacity ring holds the last N snapshots (N chosen for at
// least 250 ms of history)." At the configured 240 Hz tick rate, 250 ms is
// 60 ticks; 64 is the next power of two (common::RingBuffer requires
// one), giving a bit of margin.
namespace sim {

inline constexpr std::size_t kSnapshotRingCapacity = 64;
using SnapshotRing = common::RingBuffer<SimState, kSnapshotRingCapacity>;

}  // namespace sim
