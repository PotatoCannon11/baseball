# Header-only: POD types with no dependencies beyond <cstdint>/<array>, so
# the future sim library can depend on this without pulling in SDL,
# platform code, or anything else that would violate its isolation rules.
add_library(common INTERFACE)
target_include_directories(common INTERFACE include)
target_link_libraries(common INTERFACE baseball_compiler_flags)
