add_executable(test_alloc_counter alloc_counter_test.cpp)
target_link_libraries(test_alloc_counter PRIVATE alloc_counter)
add_test(NAME alloc_counter COMMAND test_alloc_counter)

add_executable(test_platform_memory platform_memory_test.cpp)
target_link_libraries(test_platform_memory PRIVATE platform)
add_test(NAME platform_memory COMMAND test_platform_memory)

add_executable(test_ring_buffer ring_buffer_test.cpp)
target_link_libraries(test_ring_buffer PRIVATE common)
add_test(NAME ring_buffer COMMAND test_ring_buffer)

add_executable(test_binding_table binding_table_test.cpp)
target_link_libraries(test_binding_table PRIVATE input)
add_test(NAME binding_table COMMAND test_binding_table)

add_executable(test_bias_calibration bias_calibration_test.cpp)
target_link_libraries(test_bias_calibration PRIVATE input)
add_test(NAME bias_calibration COMMAND test_bias_calibration)

add_executable(test_clip_recovery clip_recovery_test.cpp)
target_link_libraries(test_clip_recovery PRIVATE input)
add_test(NAME clip_recovery COMMAND test_clip_recovery)

add_executable(test_madgwick_filter madgwick_filter_test.cpp)
target_link_libraries(test_madgwick_filter PRIVATE input)
add_test(NAME madgwick_filter COMMAND test_madgwick_filter)

add_executable(test_bat_model bat_model_test.cpp)
target_link_libraries(test_bat_model PRIVATE input)
add_test(NAME bat_model COMMAND test_bat_model)

add_executable(test_recorder_roundtrip recorder_roundtrip_test.cpp)
target_link_libraries(test_recorder_roundtrip PRIVATE input)
add_test(NAME recorder_roundtrip COMMAND test_recorder_roundtrip)

add_executable(test_joycon_source_sdl_integration joycon_source_sdl_integration_test.cpp)
target_link_libraries(test_joycon_source_sdl_integration PRIVATE input SDL3::SDL3)
add_test(NAME joycon_source_sdl_integration COMMAND test_joycon_source_sdl_integration)

add_executable(test_mouse_keyboard_source mouse_keyboard_source_test.cpp)
target_link_libraries(test_mouse_keyboard_source PRIVATE input SDL3::SDL3)
add_test(NAME mouse_keyboard_source COMMAND test_mouse_keyboard_source)

add_executable(test_motion_pipeline motion_pipeline_test.cpp)
target_link_libraries(test_motion_pipeline PRIVATE input)
add_test(NAME motion_pipeline COMMAND test_motion_pipeline)

add_executable(test_sim_determinism sim_determinism_test.cpp)
target_link_libraries(test_sim_determinism PRIVATE sim)
add_test(NAME sim_determinism COMMAND test_sim_determinism)

add_executable(test_sim_snapshot_roundtrip sim_snapshot_roundtrip_test.cpp)
target_link_libraries(test_sim_snapshot_roundtrip PRIVATE sim)
add_test(NAME sim_snapshot_roundtrip COMMAND test_sim_snapshot_roundtrip)

add_executable(test_sim_zero_alloc sim_zero_alloc_test.cpp)
target_link_libraries(test_sim_zero_alloc PRIVATE sim alloc_counter)
add_test(NAME sim_zero_alloc COMMAND test_sim_zero_alloc)

add_executable(test_sim_collision_efficiency sim_collision_efficiency_test.cpp)
target_link_libraries(test_sim_collision_efficiency PRIVATE sim)
add_test(NAME sim_collision_efficiency COMMAND test_sim_collision_efficiency)

add_executable(test_sim_carry_distance sim_carry_distance_test.cpp)
target_link_libraries(test_sim_carry_distance PRIVATE sim)
add_test(NAME sim_carry_distance COMMAND test_sim_carry_distance)

add_executable(test_sim_timestep_convergence sim_timestep_convergence_test.cpp)
target_link_libraries(test_sim_timestep_convergence PRIVATE sim)
add_test(NAME sim_timestep_convergence COMMAND test_sim_timestep_convergence)

add_executable(test_sim_energy_audit sim_energy_audit_test.cpp)
target_link_libraries(test_sim_energy_audit PRIVATE sim)
add_test(NAME sim_energy_audit COMMAND test_sim_energy_audit)

add_executable(test_sim_monotonicity sim_monotonicity_test.cpp)
target_link_libraries(test_sim_monotonicity PRIVATE sim)
add_test(NAME sim_monotonicity COMMAND test_sim_monotonicity)

add_executable(test_pitching_machine pitching_machine_test.cpp)
target_link_libraries(test_pitching_machine PRIVATE sim)
add_test(NAME pitching_machine COMMAND test_pitching_machine)

add_executable(test_throw_release_speed throw_release_speed_test.cpp)
target_link_libraries(test_throw_release_speed PRIVATE sim)
add_test(NAME throw_release_speed COMMAND test_throw_release_speed)

add_executable(test_throw_release_timing throw_release_timing_test.cpp)
target_link_libraries(test_throw_release_timing PRIVATE sim)
add_test(NAME throw_release_timing COMMAND test_throw_release_timing)

add_executable(test_throw_spin throw_spin_test.cpp)
target_link_libraries(test_throw_spin PRIVATE sim)
add_test(NAME throw_spin COMMAND test_throw_spin)

add_executable(test_sim_ready_up sim_ready_up_test.cpp)
target_link_libraries(test_sim_ready_up PRIVATE sim)
add_test(NAME sim_ready_up COMMAND test_sim_ready_up)

add_executable(test_camera_director camera_director_test.cpp)
target_link_libraries(test_camera_director PRIVATE render)
add_test(NAME camera_director COMMAND test_camera_director)

add_executable(test_versus_role_swap versus_role_swap_test.cpp)
target_link_libraries(test_versus_role_swap PRIVATE versus sim)
add_test(NAME versus_role_swap COMMAND test_versus_role_swap)

add_executable(test_versus_calibration_persistence versus_calibration_persistence_test.cpp)
target_link_libraries(test_versus_calibration_persistence PRIVATE versus)
add_test(NAME versus_calibration_persistence COMMAND test_versus_calibration_persistence)

add_executable(test_versus_disconnect_pause versus_disconnect_pause_test.cpp)
target_link_libraries(test_versus_disconnect_pause PRIVATE versus sim)
add_test(NAME versus_disconnect_pause COMMAND test_versus_disconnect_pause)

add_executable(test_versus_hud_privacy versus_hud_privacy_test.cpp)
target_link_libraries(test_versus_hud_privacy PRIVATE versus)
target_compile_definitions(test_versus_hud_privacy PRIVATE BASEBALL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME versus_hud_privacy COMMAND test_versus_hud_privacy)

add_executable(test_versus_memory_budget versus_memory_budget_test.cpp)
target_link_libraries(test_versus_memory_budget PRIVATE versus input alloc_counter SDL3::SDL3)
add_test(NAME versus_memory_budget COMMAND test_versus_memory_budget)

add_executable(test_two_human_replay_determinism two_human_replay_determinism_test.cpp)
target_link_libraries(test_two_human_replay_determinism PRIVATE input sim)
add_test(NAME two_human_replay_determinism COMMAND test_two_human_replay_determinism)

add_executable(test_cpu_vs_cpu_sanity cpu_vs_cpu_sanity_test.cpp)
target_link_libraries(test_cpu_vs_cpu_sanity PRIVATE cpu sim common)
add_test(NAME cpu_vs_cpu_sanity COMMAND test_cpu_vs_cpu_sanity)

# Renders a few seconds headless and captures a frame -- a real GL smoke
# test (catches shader compile failures, missing GL functions, FBO setup
# bugs, crashes) rather than a pixel-perfect check, which was done by hand
# via screenshot inspection during development. Needs a working GL context
# (Mesa software rendering on Linux CI runners provides this; unverified
# on macOS/Windows CI as of writing).
#
# PASS_REGULAR_EXPRESSION requires the printed final tick count to be at
# least 3 digits (>=100) -- at the sim's 240 Hz tick rate, one simulated
# second should reach roughly 240, so this stays well clear of a healthy
# run even on a slow CI machine, while still catching a completely stuck
# tick counter (e.g. a sim loop silently paused by a wiring bug: the app
# still exits cleanly and a frame still renders even when the sim
# underneath it never advances, which is exactly what let milestone 7's
# observe_hub-on-dev-slots bug through everywhere except an actual
# interactive run -- see git history around app/game/main.cpp).
add_test(NAME render_smoke
         COMMAND game --hidden --seconds 1 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/render_smoke_test.ppm)
set_tests_properties(render_smoke PROPERTIES PASS_REGULAR_EXPRESSION "final tick=[0-9][0-9][0-9]+")
