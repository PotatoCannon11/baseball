#pragma once

#include "input/imu_types.h"

// Consumer-facing contract for "something that produces timestamped raw
// IMU/button samples": a real Joy-Con, a mouse/keyboard dev stand-in, or
// (later) a Wiimote+MotionPlus etc. slotted in via the same interface.
// Note this is NOT the sim's PlayerInput contract -- that's a separate,
// later processing stage (bias/filter/clip-recovery/quantize) built on
// top of what an ImuSource provides. See common/player_input.h.
//
// Polled once per frame by the input layer, not from inside the sim/frame
// hot loop, so a small vtable here is fine per the project's "no virtuals
// in the sim step" rule -- this lives entirely outside that step.
namespace input {

class ImuSource {
public:
    virtual ~ImuSource() = default;

    // Some sources are self-contained and need explicit pumping once per
    // frame (e.g. a scripted/replay source reading its next recorded
    // frame). Others are fed externally by a shared event dispatcher (see
    // sdl_input_hub.h) and don't need this. Default is a no-op so
    // externally-fed sources don't have to implement anything here.
    virtual void pump() {}

    virtual bool is_connected() const = 0;
    virtual const ImuRing& imu_samples() const = 0;
    virtual const ButtonRing& button_events() const = 0;
};

}  // namespace input
