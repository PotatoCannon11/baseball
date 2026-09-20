#include "versus/feedback.h"

namespace versus {

bool play_private_feedback(input::JoyconSource* gamepad, const PrivateFeedbackEvent& event) {
    if (!gamepad) return false;

    switch (event.kind) {
        case PrivateFeedbackKind::kGripRumble: {
            const RumblePattern pattern = grip_rumble_pattern(event.grip_index);
            return gamepad->rumble(pattern.low_frequency, pattern.high_frequency, pattern.duration_ms);
        }
        case PrivateFeedbackKind::kReadyConfirmation: {
            // Short uniform buzz plus a neutral white LED flash -- '&' so
            // both are attempted even if one isn't supported (a Pro
            // Controller with no LED but working rumble should still get
            // the haptic half).
            const bool rumbled = gamepad->rumble(0x3000, 0x3000, 60);
            const bool lit = gamepad->set_led(255, 255, 255);
            return rumbled || lit;
        }
    }
    return false;
}

}  // namespace versus
