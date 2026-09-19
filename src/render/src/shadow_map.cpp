#include "render/shadow_map.h"

#include <cstdio>

#include "render/gl.h"

namespace render {

bool ShadowMap::create(int size) {
    size_ = size;

    glGenTextures(1, &depth_texture_);
    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // CLAMP_TO_EDGE rather than CLAMP_TO_BORDER: avoids needing a border
    // color uniform/function for a milestone-3 scene where the light
    // frustum already comfortably covers the field, so edge-clamp
    // artifacts outside it are not visible.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "render::ShadowMap::create: framebuffer incomplete (status 0x%x)\n", status);
        destroy();
        return false;
    }
    return true;
}

void ShadowMap::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (depth_texture_) glDeleteTextures(1, &depth_texture_);
    fbo_ = depth_texture_ = 0;
    size_ = 0;
}

void ShadowMap::begin_pass() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, size_, size_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

}  // namespace render
