add_library(render STATIC
    src/gl.cpp
    src/window.cpp
    src/shader.cpp
    src/mesh.cpp
    src/shadow_map.cpp
    src/overlay.cpp
)

# khronos/glcorearb.h is included by the PUBLIC render/gl.h, so its
# include dir must be PUBLIC too. font8x8 is only ever included from
# overlay.cpp (never a public header), so it stays PRIVATE.
target_include_directories(render PUBLIC include ${CMAKE_SOURCE_DIR}/third_party/khronos)
target_include_directories(render PRIVATE ${CMAKE_SOURCE_DIR}/third_party/font8x8)
target_link_libraries(render PUBLIC baseball_compiler_flags SDL3::SDL3)
