# Overrides global operator new/delete for the whole process, so link this
# into every executable (tools, tests, and later the game itself) that
# needs allocation accounting. Depends on nothing else in this repo.
add_library(alloc_counter STATIC
    src/alloc_counter.cpp
)

target_include_directories(alloc_counter PUBLIC include)
target_link_libraries(alloc_counter PUBLIC baseball_compiler_flags)
