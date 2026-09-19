add_executable(imu_probe
    main.cpp
)

# Links against `input` (not just SDL3 directly) so it reuses the shared
# device_profile.h from src/input rather than keeping its own copy.
target_link_libraries(imu_probe PRIVATE platform input SDL3::SDL3)

if(BASEBALL_ENABLE_EVDEV AND UNIX AND NOT APPLE)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBEVDEV libevdev)
    endif()
    if(LIBEVDEV_FOUND)
        target_sources(imu_probe PRIVATE evdev_probe.cpp)
        target_compile_definitions(imu_probe PRIVATE BASEBALL_HAVE_EVDEV=1)
        target_include_directories(imu_probe PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
        target_link_libraries(imu_probe PRIVATE ${LIBEVDEV_LIBRARIES})
    else()
        message(STATUS "imu_probe: libevdev-dev not found, skipping optional evdev backend")
    endif()
endif()
