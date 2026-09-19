# Platform layer: the ONLY place in the codebase allowed to contain
# POSIX-only or Win32-only code. Everything else (sim, tools) must go
# through these small interfaces instead of #ifdef'ing per-OS code inline.
add_library(platform STATIC
    src/clock.cpp
    src/paths.cpp
    src/memory_linux.cpp
    src/memory_macos.cpp
    src/memory_windows.cpp
)

target_include_directories(platform PUBLIC include)
target_link_libraries(platform PUBLIC baseball_compiler_flags PRIVATE SDL3::SDL3)

if(WIN32)
    target_link_libraries(platform PRIVATE psapi)
endif()
