#pragma once

#include "render/shader.h"

// Dev-only debug text overlay: "bat tip speed, ball speed, spin, exit
// velocity, app allocation total, process memory, frame time, sim step
// time, tick, state hash." Renders an 8x8 monochrome bitmap font (public-
// domain, third_party/font8x8) as textured quads -- no per-frame
// std::string building: callers pass a fixed C-string buffer they've
// already formatted (e.g. via snprintf into a stack array), and each
// draw_text() call uses its own small fixed-size local vertex buffer,
// never heap allocation.
namespace render {

class Overlay {
public:
    bool create();
    void destroy();

    // Draws one line of text with its top-left corner at pixel (x, y) in
    // window coordinates (origin top-left, y grows downward). scale=1
    // renders each glyph at its native 8x8 pixels.
    void draw_text(float x, float y, const char* text, float scale, float r, float g, float b, int screen_w,
                    int screen_h);

private:
    static constexpr int kMaxCharsPerCall = 256;

    Shader shader_;
    unsigned int vao_ = 0, vbo_ = 0;
    unsigned int font_texture_ = 0;
};

}  // namespace render
