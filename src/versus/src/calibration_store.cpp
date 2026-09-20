#include "versus/calibration_store.h"

#include <cstdio>
#include <cstring>

namespace versus {

namespace {

constexpr char kMagic[8] = {'B', 'B', 'C', 'A', 'L', 'I', 'B', '1'};

struct FileHeader {
    char magic[8];
    std::uint32_t payload_size;
};

}  // namespace

std::string CalibrationStore::path_for(input::PlayerSlot slot) const {
    return pref_dir_ + "player_slot_" + std::to_string(static_cast<int>(slot)) + ".cal";
}

PlayerCalibration CalibrationStore::load(input::PlayerSlot slot) const {
    PlayerCalibration calibration;  // default-constructed fallback

    std::FILE* f = std::fopen(path_for(slot).c_str(), "rb");
    if (!f) return calibration;

    FileHeader header{};
    PlayerCalibration loaded{};
    const bool ok = std::fread(&header, sizeof(header), 1, f) == 1 &&
                     std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0 &&
                     header.payload_size == sizeof(PlayerCalibration) &&
                     std::fread(&loaded, sizeof(loaded), 1, f) == 1;
    std::fclose(f);

    return ok ? loaded : calibration;
}

bool CalibrationStore::save(input::PlayerSlot slot, const PlayerCalibration& calibration) const {
    std::FILE* f = std::fopen(path_for(slot).c_str(), "wb");
    if (!f) return false;

    FileHeader header{};
    std::memcpy(header.magic, kMagic, sizeof(kMagic));
    header.payload_size = sizeof(PlayerCalibration);

    const bool ok =
        std::fwrite(&header, sizeof(header), 1, f) == 1 && std::fwrite(&calibration, sizeof(calibration), 1, f) == 1;
    std::fclose(f);
    return ok;
}

}  // namespace versus
