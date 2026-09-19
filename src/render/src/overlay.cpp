#include "render/overlay.h"

#include <cstddef>
#include <cstring>

#include "render/gl.h"
#include "render/shaders.h"

// Public-domain 8x8 bitmap font (see third_party/font8x8/font8x8_basic.h
// header for provenance). Included only here, never in a public header,
// since it defines a non-static global array -- fine as long as exactly
// one translation unit includes it.
#include "font8x8_basic.h"

namespace render {

namespace {
struct OverlayVertex {
    float x, y, u, v;
};

constexpr int kGlyphPx = 8;
constexpr int kAtlasCols = 16;
constexpr int kAtlasRows = 8;  // 16*8 = 128 glyphs
constexpr int kAtlasW = kAtlasCols * kGlyphPx;
constexpr int kAtlasH = kAtlasRows * kGlyphPx;
}  // namespace

bool Overlay::create() {
    if (!shader_.compile(shaders::kOverlayVertex, shaders::kOverlayFragment)) return false;

    // Build the font atlas CPU-side, upload, then let the scratch buffer
    // go out of scope ("decode, upload, free the CPU copy immediately").
    {
        unsigned char pixels[kAtlasW * kAtlasH] = {};
        for (int glyph = 0; glyph < 128; ++glyph) {
            const int col = glyph % kAtlasCols;
            const int row = glyph / kAtlasCols;
            for (int y = 0; y < kGlyphPx; ++y) {
                const unsigned char bits = static_cast<unsigned char>(font8x8_basic[glyph][y]);
                for (int x = 0; x < kGlyphPx; ++x) {
                    const bool on = (bits >> x) & 1;
                    const int px = col * kGlyphPx + x;
                    const int py = row * kGlyphPx + y;
                    pixels[py * kAtlasW + px] = on ? 255 : 0;
                }
            }
        }

        glGenTextures(1, &font_texture_);
        glBindTexture(GL_TEXTURE_2D, font_texture_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    // Sized once for the worst case (kMaxCharsPerCall quads, 6 verts
    // each), GL_DYNAMIC_DRAW since it's rewritten every draw_text() call
    // via glBufferSubData -- never resized, never reallocated.
    glBufferData(GL_ARRAY_BUFFER, sizeof(OverlayVertex) * 6 * kMaxCharsPerCall, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex),
                           reinterpret_cast<void*>(offsetof(OverlayVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex),
                           reinterpret_cast<void*>(offsetof(OverlayVertex, u)));
    glBindVertexArray(0);

    return true;
}

void Overlay::destroy() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (font_texture_) glDeleteTextures(1, &font_texture_);
    vbo_ = vao_ = font_texture_ = 0;
    shader_.destroy();
}

void Overlay::draw_text(float x, float y, const char* text, float scale, float r, float g, float b, int screen_w,
                         int screen_h) {
    OverlayVertex verts[kMaxCharsPerCall * 6];
    int count = 0;
    float cursor_x = x;

    for (const char* p = text; *p != '\0' && count < kMaxCharsPerCall; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 128) continue;
        const int col = c % kAtlasCols;
        const int row = c / kAtlasCols;
        const float u0 = static_cast<float>(col * kGlyphPx) / kAtlasW;
        const float v0 = static_cast<float>(row * kGlyphPx) / kAtlasH;
        const float u1 = static_cast<float>((col + 1) * kGlyphPx) / kAtlasW;
        const float v1 = static_cast<float>((row + 1) * kGlyphPx) / kAtlasH;

        const float x0 = cursor_x, x1 = cursor_x + kGlyphPx * scale;
        const float y0 = y, y1 = y + kGlyphPx * scale;

        OverlayVertex* q = &verts[count * 6];
        q[0] = {x0, y0, u0, v0};
        q[1] = {x1, y0, u1, v0};
        q[2] = {x1, y1, u1, v1};
        q[3] = {x0, y0, u0, v0};
        q[4] = {x1, y1, u1, v1};
        q[5] = {x0, y1, u0, v1};
        ++count;

        cursor_x += kGlyphPx * scale;
    }
    if (count == 0) return;

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(OverlayVertex) * 6 * count, verts);

    shader_.use();
    glUniform2f(glGetUniformLocation(shader_.id(), "uScreenSize"), static_cast<float>(screen_w),
                static_cast<float>(screen_h));
    shader_.set_vec3("uColor", r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture_);
    shader_.set_int("uFontAtlas", 0);

    glDrawArrays(GL_TRIANGLES, 0, count * 6);
}

}  // namespace render
