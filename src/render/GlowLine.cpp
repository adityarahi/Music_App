#include "render/GlowLine.h"

#include <cmath>

namespace {

constexpr const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aNormal;
layout(location = 2) in float aSide;
layout(location = 3) in vec3 aColor;

uniform vec2 uResolution;
uniform float uHalfWidth;

out float vSide;
out vec3 vColor;

void main() {
    vec2 px = aPos + aNormal * aSide * uHalfWidth;
    gl_Position = vec4(px / uResolution * 2.0 - 1.0, 0.0, 1.0);
    vSide = aSide;
    vColor = aColor;
}
)";

constexpr const char* kFragmentSrc = R"(#version 330 core
in float vSide;
in vec3 vColor;

uniform vec3 uColor;
uniform float uAlpha;
uniform float uFalloff;

out vec4 fragColor;

void main() {
    float edge = clamp(1.0 - abs(vSide), 0.0, 1.0);
    fragColor = vec4(uColor * vColor, uAlpha * pow(edge, uFalloff));
}
)";

}  // namespace

GlowLine::~GlowLine() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool GlowLine::init() {
    if (!shader_.build(kVertexSrc, kFragmentSrc)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, side)));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    return true;
}

void GlowLine::setPoints(const std::vector<Vec2>& points, bool closed, const std::vector<Rgb>* colors) {
    vertices_.clear();
    const std::size_t n = points.size();
    if (n < 2) return;

    const std::size_t count = closed ? n + 1 : n;  // repeat the first point to close the strip
    for (std::size_t c = 0; c < count; ++c) {
        const std::size_t i = c % n;
        // Neighbours wrap around on a closed loop and clamp at the ends of an open line.
        const std::size_t prevIdx = i > 0 ? i - 1 : (closed ? n - 1 : 0);
        const std::size_t nextIdx = i + 1 < n ? i + 1 : (closed ? 0 : n - 1);
        const Vec2& p = points[i];
        const Vec2& prev = points[prevIdx];
        const Vec2& next = points[nextIdx];

        float tx = next.x - prev.x;
        float ty = next.y - prev.y;
        const float len = std::sqrt(tx * tx + ty * ty);
        if (len > 0.0f) {
            tx /= len;
            ty /= len;
        } else {
            tx = 1.0f;
            ty = 0.0f;
        }

        const Rgb col = colors && i < colors->size() ? (*colors)[i] : Rgb{1.0f, 1.0f, 1.0f};
        vertices_.push_back({p.x, p.y, -ty, tx, -1.0f, col.r, col.g, col.b});
        vertices_.push_back({p.x, p.y, -ty, tx, +1.0f, col.r, col.g, col.b});
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);
}

void GlowLine::draw(float targetWidth, float targetHeight, float halfWidth, float falloff, const float* rgb, float alpha) {
    if (vertices_.empty()) return;

    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), targetWidth, targetHeight);
    glUniform1f(shader_.uniform("uHalfWidth"), halfWidth);
    glUniform1f(shader_.uniform("uFalloff"), falloff);
    glUniform3fv(shader_.uniform("uColor"), 1, rgb);
    glUniform1f(shader_.uniform("uAlpha"), alpha);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
}
