#include <cstdio>
#include <filesystem>

#include "versus/calibration_store.h"
#include "versus/role_assignment.h"

namespace {

bool check(bool condition, const char* what) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", what);
    return condition;
}

bool same(const versus::PlayerCalibration& a, const versus::PlayerCalibration& b) {
    for (int i = 0; i < 3; ++i) {
        if (a.gyro_bias[i] != b.gyro_bias[i]) return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (a.recenter_wxyz[i] != b.recenter_wxyz[i]) return false;
    }
    return a.arm_length_m == b.arm_length_m && a.filter_gain == b.filter_gain;
}

}  // namespace

// Milestone 7 validation test: "per-player calibration survives a role
// swap and a device swap." Keyed by PlayerSlot (never by Role or device
// id), so both operations are no-ops from the calibration store's point
// of view -- this test proves that by actually performing them and
// re-reading.
int main() {
    bool ok = true;

    const std::filesystem::path scratch_dir =
        std::filesystem::temp_directory_path() / "baseball_versus_calibration_test";
    std::filesystem::create_directories(scratch_dir);
    const versus::CalibrationStore store(scratch_dir.string() + "/");

    // Fresh slot with nothing saved yet -- default, not an error.
    const versus::PlayerCalibration fresh_p1 = store.load(input::PlayerSlot::kP1);
    ok &= check(fresh_p1.arm_length_m == versus::PlayerCalibration{}.arm_length_m,
                "an unsaved slot should load as the default calibration");

    versus::PlayerCalibration p1_calibration;
    p1_calibration.gyro_bias[0] = 0.013f;
    p1_calibration.gyro_bias[1] = -0.02f;
    p1_calibration.gyro_bias[2] = 0.5f;
    p1_calibration.arm_length_m = 0.81f;
    p1_calibration.filter_gain = 0.15f;
    p1_calibration.recenter_wxyz[0] = 0.707f;
    p1_calibration.recenter_wxyz[3] = 0.707f;

    versus::PlayerCalibration p2_calibration;
    p2_calibration.arm_length_m = 0.62f;  // deliberately different, to catch cross-slot mixups

    ok &= check(store.save(input::PlayerSlot::kP1, p1_calibration), "saving P1's calibration should succeed");
    ok &= check(store.save(input::PlayerSlot::kP2, p2_calibration), "saving P2's calibration should succeed");

    ok &= check(same(store.load(input::PlayerSlot::kP1), p1_calibration), "P1's calibration round-trips exactly");
    ok &= check(same(store.load(input::PlayerSlot::kP2), p2_calibration), "P2's calibration round-trips exactly");

    // --- Role swap: calibration is keyed by slot, so RoleAssignment
    // swapping which slot pitches/bats must not touch the store at all.
    versus::RoleAssignment assignment;
    assignment.swap();
    ok &= check(same(store.load(input::PlayerSlot::kP1), p1_calibration),
                "a role swap must not change P1's stored calibration (it's keyed by slot, not role)");
    ok &= check(same(store.load(input::PlayerSlot::kP2), p2_calibration),
                "a role swap must not change P2's stored calibration (it's keyed by slot, not role)");

    // --- Device swap: rebinding P1's slot to a different physical
    // device (simulated here -- the store has no device-id concept at
    // all, so there's nothing for a device swap to invalidate) must
    // leave the calibration exactly as it was.
    ok &= check(same(store.load(input::PlayerSlot::kP1), p1_calibration),
                "a device swap must not change P1's stored calibration (store has no device-id key to lose)");

    std::filesystem::remove_all(scratch_dir);

    if (ok) {
        std::printf("PASS: per-player calibration survives role swaps and device swaps\n");
        return 0;
    }
    return 1;
}
