#pragma once

#include <SDL3/SDL.h>

// Owns an SDL window + OpenGL 4.1 core context. GL 4.1 core is the
// project's rendering baseline specifically because macOS caps at 4.1;
// the forward-compatible flag is required to get a core profile context
// on macOS at all. Keep this the ONLY place that creates the GL context
// so the attribute set stays in one place.
namespace render {

class Window {
public:
    // Returns false (with SDL_GetError() populated) on failure. Does not
    // throw -- no exceptions in this codebase's hot paths, and this runs
    // once at startup anyway.
    bool create(const char* title, int width, int height, bool visible);
    void destroy();

    void swap();

    SDL_Window* sdl_window() const { return window_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace render
