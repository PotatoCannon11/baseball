#include "input/recorder.h"

#include <cstdio>
#include <cstring>

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

}  // namespace

// Milestone 2 deliverable: "record and replay PlayerInput streams from a
// file." This is the check that matters most for the eventual "recorded
// real swings become regression tests" use case -- it must round-trip
// bit-for-bit, since a regression test that quietly drifted a bit each
// re-recording wouldn't be much of a regression test.
int main() {
    bool ok = true;
    const char* path = "recorder_roundtrip_test.bin";

    constexpr int kFrameCount = 500;
    common::PlayerInput written[kFrameCount];
    for (int i = 0; i < kFrameCount; ++i) {
        common::PlayerInput& p = written[i];
        p.sequence = static_cast<std::uint16_t>(i);
        p.tick = static_cast<std::uint32_t>(i * 2 + 1);
        for (int j = 0; j < 4; ++j) p.orientation[j] = static_cast<std::int16_t>(i * 7 + j - 1000);
        for (int j = 0; j < 3; ++j) p.angular_velocity[j] = static_cast<std::int16_t>(i * 3 - j);
        p.stick_x = static_cast<std::int16_t>(i - 250);
        p.stick_y = static_cast<std::int16_t>(250 - i);
        p.buttons = static_cast<std::uint16_t>(i & 0xFF);
        p.clip_flags = static_cast<std::uint8_t>(i & 0x3F);
    }

    {
        input::PlayerInputRecorder recorder;
        ok &= check(recorder.open(path), "recorder should open a writable path");
        for (int i = 0; i < kFrameCount; ++i) recorder.write(written[i]);
        ok &= check(recorder.frames_written() == kFrameCount, "frames_written should match the number of write() calls");
    }  // destructor closes the file

    {
        input::PlayerInputReplayer replayer;
        ok &= check(replayer.open(path), "replayer should open the file the recorder just wrote");

        int i = 0;
        common::PlayerInput read_back;
        while (replayer.next(&read_back)) {
            if (i >= kFrameCount) {
                ok &= check(false, "replayer produced more frames than were written");
                break;
            }
            const bool identical = std::memcmp(&read_back, &written[i], sizeof(common::PlayerInput)) == 0;
            ok &= check(identical, "replayed frame should be bit-for-bit identical to what was written");
            ++i;
        }
        ok &= check(i == kFrameCount, "replayer should produce exactly as many frames as were written");
    }

    // A file with the wrong magic/version should be rejected rather than
    // silently misread.
    {
        std::FILE* f = std::fopen("recorder_roundtrip_test_garbage.bin", "wb");
        ok &= check(f != nullptr, "should be able to create the garbage test file");
        if (f) {
            const char garbage[16] = {0};
            std::fwrite(garbage, sizeof(garbage), 1, f);
            std::fclose(f);
        }

        input::PlayerInputReplayer replayer;
        ok &= check(!replayer.open("recorder_roundtrip_test_garbage.bin"),
                    "replayer should refuse a file that isn't a valid recording");
    }

    if (ok) {
        std::printf("PASS: recorded PlayerInput stream replays bit-for-bit identical to what was written\n");
        return 0;
    }
    return 1;
}
