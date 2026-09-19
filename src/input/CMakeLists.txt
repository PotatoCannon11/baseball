add_library(input STATIC
    src/madgwick_filter.cpp
    src/motion_pipeline.cpp
    src/recorder.cpp
    src/joycon_source.cpp
    src/mouse_keyboard_source.cpp
    src/sdl_input_hub.cpp
)

target_include_directories(input PUBLIC include)
# SDL3 is PUBLIC, not PRIVATE: several of this library's public headers
# (joycon_source.h, mouse_keyboard_source.h, sdl_input_hub.h) expose SDL
# types (SDL_Gamepad*, SDL_Event) directly, so consumers need SDL3's
# include paths too.
target_link_libraries(input PUBLIC common baseball_compiler_flags SDL3::SDL3)
