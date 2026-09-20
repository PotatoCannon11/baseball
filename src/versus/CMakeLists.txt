# Milestone 7: local two-player-on-one-screen logic (join flow, role
# swap, ready-up-adjacent orchestration, per-player calibration
# persistence, private controller feedback, shared-HUD privacy). Depends
# on input (device binding/rumble/LED) + sim (PlayerSlot-agnostic
# SimState/step) + platform (pref path) + common, but this is presentation
# / orchestration glue, not sim -- it must never be linked into the sim
# target itself, matching the same isolation rule sim/CMakeLists.txt
# enforces in the other direction.
add_library(versus STATIC
    src/calibration_store.cpp
    src/join_flow.cpp
    src/feedback.cpp
)

target_include_directories(versus PUBLIC include)
target_link_libraries(versus PUBLIC sim input common baseball_compiler_flags)
