cmake_minimum_required(VERSION 3.24)

project(baseball LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # no compiler-specific extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(BASEBALL_ENABLE_EVDEV "Build the optional Linux evdev IMU backend" ON)
option(BASEBALL_BUILD_TESTS "Build the test suite" ON)
option(BASEBALL_BUILD_TOOLS "Build measurement/dev tools" ON)

# --- Shared compiler flags -------------------------------------------------
# These apply project-wide. The determinism-sensitive flags (-ffp-contract=off,
# no -ffast-math, no -march=native) matter most for the future "sim" library,
# but there is no reason to diverge for the rest of the code in milestone 1.
add_library(baseball_compiler_flags INTERFACE)
target_compile_features(baseball_compiler_flags INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(baseball_compiler_flags INTERFACE
        -Wall -Wextra
        -ffp-contract=off   # forbid contracting a*b+c into a single fused FMA
        -fno-fast-math
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(baseball_compiler_flags INTERFACE
        /W4
        /fp:precise         # MSVC equivalent of forbidding FMA contraction
    )
endif()

include(FetchContent)

# SDL3 is the primary input/window/gamepad-sensor dependency. Built from
# source via FetchContent so the exact same version is used on every OS in
# CI, rather than depending on whatever package managers happen to ship.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

add_subdirectory(src/platform)
add_subdirectory(src/alloc)
add_subdirectory(src/common)
add_subdirectory(src/input)
add_subdirectory(src/sim)
add_subdirectory(src/cpu)
add_subdirectory(src/render)
add_subdirectory(src/versus)

add_subdirectory(app/game)

if(BASEBALL_BUILD_TOOLS)
    add_subdirectory(tools/imu_probe)
    add_subdirectory(tools/barrel_speed)
    add_subdirectory(tools/sim_headless)
    add_subdirectory(tools/cpu_vs_cpu)
endif()

if(BASEBALL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
