#include "versus/join_flow.h"

#include "input/sdl_input_hub.h"

namespace versus {

void observe_hub(JoinFlow* flow, input::SdlInputHub& hub) {
    for (input::PlayerSlot slot : {input::PlayerSlot::kP1, input::PlayerSlot::kP2}) {
        // hub.joycon_for_slot() already returns null both for "never
        // claimed" and "claimed but the device just dropped" (the
        // binding table auto-releases on a real SDL device-removed
        // event) -- JoinFlow's own remembered state (kActive vs.
        // kUnclaimed) is what distinguishes those two cases here.
        const bool connected = hub.joycon_for_slot(slot) != nullptr;
        if (!connected && flow->state(slot) == SlotState::kActive) {
            flow->mark_disconnected(slot);
        } else if (connected && flow->state(slot) == SlotState::kDisconnected) {
            flow->mark_reconnected(slot);
        }
    }
}

sim::SimState step_or_pause(const sim::SimState& prev, const sim::SimInputs& inputs, const sim::SimConfig& config,
                             const JoinFlow& flow, sim::StepEvents* events) {
    if (flow.should_pause()) {
        if (events) *events = sim::StepEvents{};
        return prev;
    }
    return sim::step(prev, inputs, config, events);
}

}  // namespace versus
