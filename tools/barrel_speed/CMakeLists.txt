add_executable(barrel_speed
    main.cpp
)

target_link_libraries(barrel_speed PRIVATE input platform SDL3::SDL3)
