#include "input/recorder.h"

#include <cstdio>
#include <cstring>

namespace input {

namespace {

constexpr char kMagic[8] = {'B', 'B', 'P', 'I', 'N', 'P', 'U', 'T'};

struct FileHeader {
    char magic[8];
    std::uint16_t player_input_version;
    std::uint16_t player_input_size;
    std::uint32_t reserved = 0;
};

}  // namespace

bool PlayerInputRecorder::open(const std::string& path) {
    close();
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    FileHeader header{};
    std::memcpy(header.magic, kMagic, sizeof(kMagic));
    header.player_input_version = common::PlayerInput::kVersion;
    header.player_input_size = static_cast<std::uint16_t>(sizeof(common::PlayerInput));
    if (std::fwrite(&header, sizeof(header), 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    file_ = f;
    frames_written_ = 0;
    return true;
}

void PlayerInputRecorder::write(const common::PlayerInput& frame) {
    if (!file_) return;
    std::fwrite(&frame, sizeof(frame), 1, static_cast<std::FILE*>(file_));
    ++frames_written_;
}

void PlayerInputRecorder::close() {
    if (file_) {
        std::fclose(static_cast<std::FILE*>(file_));
        file_ = nullptr;
    }
}

bool PlayerInputReplayer::open(const std::string& path) {
    close();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    FileHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1 ||
        std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0 ||
        header.player_input_version != common::PlayerInput::kVersion ||
        header.player_input_size != sizeof(common::PlayerInput)) {
        std::fclose(f);
        return false;
    }

    file_ = f;
    return true;
}

bool PlayerInputReplayer::next(common::PlayerInput* out) {
    if (!file_) return false;
    return std::fread(out, sizeof(*out), 1, static_cast<std::FILE*>(file_)) == 1;
}

void PlayerInputReplayer::close() {
    if (file_) {
        std::fclose(static_cast<std::FILE*>(file_));
        file_ = nullptr;
    }
}

}  // namespace input
