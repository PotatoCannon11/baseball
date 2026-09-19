#pragma once

// One depth-only shadow map, reused every frame -- per spec: "One depth-
// only shadow map (start 2048^2)... the ball's shadow is the key depth
// cue." Fixed size, allocated once (glTexImage2D at create()), never
// resized: no per-frame texture allocation.
namespace render {

class ShadowMap {
public:
    bool create(int size);
    void destroy();

    // Binds the shadow FBO and sets the viewport to size x size. Caller
    // must restore the default framebuffer/viewport afterward (see
    // Renderer, which does this around the shadow pass).
    void begin_pass() const;

    unsigned int depth_texture() const { return depth_texture_; }
    int size() const { return size_; }

private:
    unsigned int fbo_ = 0;
    unsigned int depth_texture_ = 0;
    int size_ = 0;
};

}  // namespace render
