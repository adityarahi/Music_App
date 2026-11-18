#include "viz/Oscilloscope.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aNormal;
layout(location = 2) in float aSide;

uniform vec2 uResolution;
uniform float uHalfWidth;

out float vSide;

void main() {
    vec2 px = aPos + aNormal * aSide * uHalfWidth;
    gl_Position = vec4(px / uResolution * 2.0 - 1.0, 0.0, 1.0);
    vSide = aSide;
}
)";

constexpr const char* kFragmentSrc = R"(#version 330 core
in float vSide;

uniform vec3 uColor;
uniform float uAlpha;
uniform float uFalloff;

out vec4 fragColor;

void main() {
    float edge = clamp(1.0 - abs(vSide), 0.0, 1.0);
    fragColor = vec4(uColor, uAlpha * pow(edge, uFalloff));
}
)";

void hsvToRgb(float h, float s, float v, float* rgb) {
    h = h - std::floor(h);
    for (int i = 0; i < 3; ++i) {
        const float k = std::fmod(5.0f - 2.0f * static_cast<float>(i) + h * 6.0f, 6.0f);
        rgb[i] = v - v * s * std::clamp(std::min(k, 4.0f - k), 0.0f, 1.0f);
    }
}

}  // namespace

Oscilloscope::~Oscilloscope() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool Oscilloscope::init(int width, int height) {
    if (!shader_.build(kVertexSrc, kFragmentSrc)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, side)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    vertices_.reserve(AudioFeatures::kWaveformSize * 4);
    resize(width, height);
    return true;
}

void Oscilloscope::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
}

void Oscilloscope::update(const AudioFeatures& features, float dt) {
    left_ = features.waveformLeft;
    right_ = features.waveformRight;
    level_ = features.level;
    time_ += dt;
}

void Oscilloscope::appendTrace(const float* samples, std::size_t count, float centerY, float amplitude) {
    const float w = static_cast<float>(width_);
    const float step = w / static_cast<float>(count - 1);

    auto point = [&](std::size_t i) {
        return std::array<float, 2>{static_cast<float>(i) * step, centerY + samples[i] * amplitude};
    };

    for (std::size_t i = 0; i < count; ++i) {
        const auto prev = point(i == 0 ? 0 : i - 1);
        const auto next = point(i + 1 == count ? i : i + 1);
        const auto p = point(i);

        float tx = next[0] - prev[0];
        float ty = next[1] - prev[1];
        const float len = std::sqrt(tx * tx + ty * ty);
        tx /= len;
        ty /= len;

        vertices_.push_back({p[0], p[1], -ty, tx, -1.0f});
        vertices_.push_back({p[0], p[1], -ty, tx, +1.0f});
    }
}

void Oscilloscope::drawPass(float halfWidth, float falloff, const float* rgb, const float* rgb2, float alpha) {
    const auto perTrace = static_cast<GLsizei>(vertices_.size() / 2);

    glUniform1f(shader_.uniform("uHalfWidth"), halfWidth);
    glUniform1f(shader_.uniform("uFalloff"), falloff);
    glUniform1f(shader_.uniform("uAlpha"), alpha);

    glUniform3fv(shader_.uniform("uColor"), 1, rgb);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, perTrace);
    glUniform3fv(shader_.uniform("uColor"), 1, rgb2);
    glDrawArrays(GL_TRIANGLE_STRIP, perTrace, perTrace);
}

void Oscilloscope::render() {
    const float h = static_cast<float>(height_);
    const float amplitude = h * 0.22f;
    const std::size_t n = left_.size();

    vertices_.clear();
    appendTrace(left_.data(), n, h * 0.70f, amplitude);
    appendTrace(right_.data(), n, h * 0.30f, amplitude);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);

    // Hue drifts slowly on its own and jumps ahead a little with loudness.
    const float energy = std::min(level_ * 3.0f, 1.0f);
    const float hue = time_ * 0.03f + energy * 0.08f;
    float colorL[3], colorR[3];
    hsvToRgb(hue, 0.75f, 1.0f, colorL);
    hsvToRgb(hue + 0.12f, 0.75f, 1.0f, colorR);

    const float scale = h / 720.0f;  // keep line widths proportional to window size

    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), static_cast<float>(width_), h);
    glBindVertexArray(vao_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive: overlapping light adds up into glow

    drawPass((14.0f + 18.0f * energy) * scale, 2.5f, colorL, colorR, 0.35f + 0.4f * energy);
    drawPass(2.2f * scale, 0.6f, colorL, colorR, 1.0f);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}
