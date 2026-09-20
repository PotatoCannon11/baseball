add_executable(game
    main.cpp
)

target_link_libraries(game PRIVATE sim render versus input platform alloc_counter SDL3::SDL3)
