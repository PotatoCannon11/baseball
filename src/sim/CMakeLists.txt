# The sim library. Per the architecture spec, this must depend on NOTHING
# but the C++ standard library and its own small math wrapper -- no SDL, no
# GL, no platform headers, no clocks, no filesystem, no input devices. This
# is enforced here simply by never linking anything else in; do not add
# target_link_libraries(sim ... SDL3::SDL3 / platform / input) no matter
# how tempting a shortcut it seems.
add_library(sim STATIC
    src/step.cpp
    src/hash.cpp
    src/flight.cpp
    src/collision.cpp
    src/pitching_machine.cpp
    src/throw.cpp
)

target_include_directories(sim PUBLIC include)
target_link_libraries(sim PUBLIC common baseball_compiler_flags)
