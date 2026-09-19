# CPU player logic: pitcher/batter AI and at-bat classification. Depends
# on sim (to read SimState and reuse flight integration) and common (for
# PlayerInput), but never on SDL/platform/input -- a CPU player is just
# another PlayerInput producer, exactly like a human input source, and is
# usable from a headless batch tool with no window at all.
add_library(cpu STATIC
    src/pitcher_ai.cpp
    src/batter_ai.cpp
    src/at_bat.cpp
)

target_include_directories(cpu PUBLIC include)
target_link_libraries(cpu PUBLIC sim common baseball_compiler_flags)
