add_executable(sim_headless
    main.cpp
)

target_link_libraries(sim_headless PRIVATE sim alloc_counter platform)
