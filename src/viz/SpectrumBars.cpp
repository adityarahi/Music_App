#include "viz/SpectrumBars.h"

#include "render/Color.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform vec2 uResolution;

out vec4 vColor;

void main() {
    gl_Position = vec4(aPos / uResolution * 2.0 - 1.0, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr const char* kFragmentSrc = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;

void main() {
    fragColor = vColor;
}
)";

constexpr float kPeakHoldSeconds = 0.35f;
constexpr float kPeakGravity = 2.2f;   // screen-heights per second squared

}  // namespace

SpectrumBars::~SpectrumBars() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool SpectrumBars::init(int width, int height) {
    if (!shader_.build(kVertexSrc, kFragmentSrc)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    vertices_.reserve(kBars * 6 * 4);  // glow, bar, reflection, cap
    resize(width, height);
    return true;
}

void SpectrumBars::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
}

void SpectrumBars::update(const AudioFeatures& features, float dt) {
    heights_ = features.bands;
    beatPulse_ = features.beatPulse;
    time_ += dt;

    // Peak caps: snap up to the bar, hang for a moment, then fall with gravity.
    for (std::size_t i = 0; i < kBars; ++i) {
        if (heights_[i] >= peaks_[i]) {
            peaks_[i] = heights_[i];
            peakVelocity_[i] = 0.0f;
            peakHold_[i] = kPeakHoldSeconds;
        } else if (peakHold_[i] > 0.0f) {
            peakHold_[i] -= dt;
        } else {
            peakVelocity_[i] += kPeakGravity * dt;
            peaks_[i] = std::max(heights_[i], peaks_[i] - peakVelocity_[i] * dt);
        }
    }
}

void SpectrumBars::addQuad(float x0, float y0, float x1, float y1, Color bottom, Color top) {
    const Vertex bl{x0, y0, bottom.r, bottom.g, bottom.b, bottom.a};
    const Vertex br{x1, y0, bottom.r, bottom.g, bottom.b, bottom.a};
    const Vertex tl{x0, y1, top.r, top.g, top.b, top.a};
    const Vertex tr{x1, y1, top.r, top.g, top.b, top.a};
    vertices_.insert(vertices_.end(), {bl, br, tr, bl, tr, tl});
}

void SpectrumBars::render() {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float scale = h / 720.0f;

    const float margin = w * 0.06f;
    const float slot = (w - 2.0f * margin) / static_cast<float>(kBars);
    const float barWidth = slot * 0.72f;
    const float baseline = h * 0.30f;
    const float maxHeight = h * 0.58f;
    const float glowPad = 6.0f * scale;
    const float capHeight = 4.0f * scale;
    const float capGap = 3.0f * scale;

    const float flash = 1.0f + 0.35f * beatPulse_;
    const float baseHue = time_ * 0.02f;

    vertices_.clear();
    for (std::size_t i = 0; i < kBars; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kBars - 1);
        const float x0 = margin + static_cast<float>(i) * slot + (slot - barWidth) * 0.5f;
        const float x1 = x0 + barWidth;
        const float barTop = baseline + std::max(heights_[i] * maxHeight, 2.0f * scale);
        const float capY = baseline + peaks_[i] * maxHeight + capGap;

        // Bass on the left warm, treble on the right cool; the whole palette drifts.
        float deep[3], bright[3];
        hsvToRgb(baseHue + t * 0.45f, 0.95f, 0.45f, deep);
        hsvToRgb(baseHue + t * 0.45f + 0.06f, 0.55f, std::min(1.0f, 0.95f * flash), bright);

        const Color bottom{deep[0], deep[1], deep[2], 1.0f};
        const Color top{bright[0], bright[1], bright[2], 1.0f};
        const Color glow{bright[0], bright[1], bright[2], 0.12f + 0.25f * heights_[i] * flash};
        const Color clear{bright[0], bright[1], bright[2], 0.0f};

        // Soft halo around the bar, then the bar itself (additive, so they blend into light).
        addQuad(x0 - glowPad, baseline, x1 + glowPad, barTop + glowPad, glow, clear);
        addQuad(x0, baseline, x1, barTop, bottom, top);

        // Reflection: a shorter, fading copy below the baseline.
        const float reflection = (barTop - baseline) * 0.35f;
        addQuad(x0, baseline - reflection, x1, baseline,
                Color{deep[0], deep[1], deep[2], 0.0f}, Color{bright[0], bright[1], bright[2], 0.22f});

        addQuad(x0, capY, x1, capY + capHeight, Color{1.0f, 1.0f, 1.0f, 0.9f}, Color{bright[0], bright[1], bright[2], 0.9f});
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);

    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), w, h);
    glBindVertexArray(vao_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}
