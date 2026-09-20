#include "render/window.h"

#include <cstdio>

#include "render/gl.h"

namespace render {

bool Window::create(const char* title, int width, int height, bool visible) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // Required to get a core-profile context on macOS at all.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | (visible ? 0u : SDL_WINDOW_HIDDEN);
    window_ = SDL_CreateWindow(title, width, height, flags);
    if (!window_) {
        std::fprintf(stderr, "render::Window::create: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // Belt-and-suspenders alongside SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH: a
    // freshly-created window isn't guaranteed to already be the focused/
    // frontmost one on every platform/launch context (e.g. launched from
    // a script rather than double-clicked), and keyboard input in
    // particular needs real focus, not just click-through. No-op if
    // already focused; harmless on a hidden window (headless/test runs).
    if (visible) SDL_RaiseWindow(window_);

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::fprintf(stderr, "render::Window::create: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    if (!SDL_GL_MakeCurrent(window_, gl_context_)) {
        std::fprintf(stderr, "render::Window::create: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        destroy();
        return false;
    }

    if (!gl_load()) {
        std::fprintf(stderr, "render::Window::create: failed to load required GL functions\n");
        destroy();
        return false;
    }

    SDL_GL_SetSwapInterval(1);  // vsync; best-effort, not fatal if unsupported

    width_ = width;
    height_ = height;
    return true;
}

void Window::destroy() {
    if (gl_context_) {
        SDL_GL_DestroyContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void Window::swap() {
    if (window_) SDL_GL_SwapWindow(window_);
}

}  // namespace render
