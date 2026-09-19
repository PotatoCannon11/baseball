#pragma once

#include <cstddef>
#include <string>

#include "common/player_input.h"

// "Input recording: record and replay PlayerInput streams from a file
// (fixed buffers, no per-sample allocation)." Each frame is written/read
// individually via stdio, not buffered up in memory, so a long recording
// doesn't grow process memory.
//
// This is a local recording format only -- NOT the eventual network wire
// format. The architecture spec is explicit that raw struct bytes must
// never go over a real network wire; that rule is about peer-to-peer
// transmission, not this local memcpy-based recording used for replay and
// regression tests.
namespace input {

class PlayerInputRecorder {
public:
    ~PlayerInputRecorder() { close(); }

    bool open(const std::string& path);
    void write(const common::PlayerInput& frame);
    void close();

    bool is_open() const { return file_ != nullptr; }
    std::size_t frames_written() const { return frames_written_; }

private:
    void* file_ = nullptr;
    std::size_t frames_written_ = 0;
};

class PlayerInputReplayer {
public:
    ~PlayerInputReplayer() { close(); }

    // Fails if the file doesn't exist, has the wrong magic, or was written
    // by a build with a different PlayerInput layout/version -- refuses
    // rather than silently misinterpreting bytes.
    bool open(const std::string& path);
    bool next(common::PlayerInput* out);
    void close();

    bool is_open() const { return file_ != nullptr; }

private:
    void* file_ = nullptr;
};

}  // namespace input
