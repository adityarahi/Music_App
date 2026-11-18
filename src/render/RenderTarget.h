#pragma once

#include <glad/gl.h>

// An offscreen framebuffer with one colour texture. Move-only.
class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget() { destroy(); }

    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // GL_RGBA16F is worth it for feedback effects: with 8-bit colour, slowly
    // fading trails get stuck at a faint non-zero value instead of reaching black.
    bool create(int width, int height, GLenum internalFormat = GL_RGBA8);
    void destroy();

    // Binds for drawing and sets the viewport to cover the whole target.
    void bind() const;
    void clear(float r, float g, float b, float a) const;

    GLuint texture() const { return texture_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    GLuint fbo_ = 0;
    GLuint texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Remembers the bound draw framebuffer and viewport, restoring them when it
// goes out of scope, so an effect can use its own targets internally and
// still draw its final image wherever the caller pointed it.
class FramebufferScope {
public:
    FramebufferScope();
    ~FramebufferScope() { restore(); }

    FramebufferScope(const FramebufferScope&) = delete;
    FramebufferScope& operator=(const FramebufferScope&) = delete;

    void restore() const;

private:
    GLint framebuffer_ = 0;
    GLint viewport_[4] = {};
};
