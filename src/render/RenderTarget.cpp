#include "render/RenderTarget.h"

#include <cstdio>
#include <utility>

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : fbo_(std::exchange(other.fbo_, 0)),
      texture_(std::exchange(other.texture_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        fbo_ = std::exchange(other.fbo_, 0);
        texture_ = std::exchange(other.texture_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

bool RenderTarget::create(int width, int height, GLenum internalFormat) {
    destroy();
    if (width <= 0 || height <= 0) return false;

    GLint previous = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    const GLenum type = internalFormat == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0, GL_RGBA, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Render target %dx%d incomplete (0x%x)\n", width, height, status);
        destroy();
        return false;
    }

    width_ = width;
    height_ = height;
    clear(0.0f, 0.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    return true;
}

void RenderTarget::destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (texture_) glDeleteTextures(1, &texture_);
    fbo_ = texture_ = 0;
    width_ = height_ = 0;
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

void RenderTarget::clear(float r, float g, float b, float a) const {
    bind();
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

FramebufferScope::FramebufferScope() {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer_);
    glGetIntegerv(GL_VIEWPORT, viewport_);
}

void FramebufferScope::restore() const {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer_));
    glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
}
