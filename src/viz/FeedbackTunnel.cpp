#include "viz/FeedbackTunnel.h"

#include "render/Color.h"
#include "render/Fullscreen.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

// Resamples the previous frame: zoom toward the centre, rotate, shift hue, fade.
constexpr const char* kWarpFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uPrevious;
uniform vec2 uAspect;     // (width / height, 1) so rotation stays circular
uniform float uZoom;      // > 1 pulls the old image outward
uniform float uAngle;
uniform float uFade;
uniform float uHueShift;

out vec4 fragColor;

vec3 rotateHue(vec3 c, float angle) {
    const vec3 k = vec3(0.57735);   // grey axis
    float ca = cos(angle);
    return c * ca + cross(k, c) * sin(angle) + k * dot(k, c) * (1.0 - ca);
}

void main() {
    vec2 p = (vUv - 0.5) * uAspect;
    float c = cos(uAngle), s = sin(uAngle);
    p = mat2(c, s, -s, c) * p / uZoom;
    vec2 uv = p / uAspect + 0.5;

    vec3 color = texture(uPrevious, uv).rgb;
    color = max(rotateHue(color, uHueShift), 0.0) * uFade;
    fragColor = vec4(color, 1.0);
}
)";

// Copies the trails out: soft-clips bright areas (so overlapping trails
// saturate gracefully instead of blowing out to white) and darkens the corners.
constexpr const char* kPresentFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uImage;

out vec4 fragColor;

void main() {
    vec3 color = texture(uImage, vUv).rgb;
    color = 1.0 - exp(-color * 1.2);
    vec2 d = vUv - 0.5;
    color *= 1.0 - 0.6 * dot(d, d);
    fragColor = vec4(color, 1.0);
}
)";

constexpr std::size_t kRingPoints = 256;

}  // namespace

bool FeedbackTunnel::init(int width, int height) {
    if (!warp_.build(kFullscreenVertexSrc, kWarpFragmentSrc)) return false;
    if (!present_.build(kFullscreenVertexSrc, kPresentFragmentSrc)) return false;
    if (!ring_.init()) return false;
    points_.reserve(kRingPoints);
    resize(width, height);
    return history_[0].texture() != 0;
}

void FeedbackTunnel::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    for (auto& target : history_)
        target.create(width_, height_, GL_RGBA16F);
}

void FeedbackTunnel::update(const AudioFeatures& features, float dt) {
    for (std::size_t i = 0; i < mono_.size(); ++i)
        mono_[i] = 0.5f * (features.waveformLeft[i] + features.waveformRight[i]);
    bass_ = features.bass;
    mid_ = features.mid;
    level_ = features.level;
    beatPulse_ = features.beatPulse;
    time_ += dt;
    dt_ = dt;
    spin_ += dt * (0.2f + 0.8f * mid_);
}

void FeedbackTunnel::buildRing() {
    const float cx = static_cast<float>(width_) * 0.5f;
    const float cy = static_cast<float>(height_) * 0.5f;
    const float h = static_cast<float>(height_);

    const float baseRadius = h * (0.12f + 0.06f * bass_ + 0.04f * beatPulse_);
    const float wobble = h * 0.10f;
    const std::size_t samplesPerPoint = mono_.size() / kRingPoints;

    // Each ring point averages a few samples so the ring isn't too spiky.
    std::array<float, kRingPoints> values{};
    for (std::size_t i = 0; i < kRingPoints; ++i) {
        float sum = 0.0f;
        for (std::size_t j = 0; j < samplesPerPoint; ++j)
            sum += mono_[i * samplesPerPoint + j];
        values[i] = sum / static_cast<float>(samplesPerPoint);
    }

    // The waveform's start and end don't match up; blend the last stretch
    // toward the first value so the loop has no visible seam.
    constexpr std::size_t kSeam = 24;
    for (std::size_t i = 0; i < kSeam; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(kSeam + 1);
        float& v = values[kRingPoints - kSeam + i];
        v = v * (1.0f - t) + values[0] * t;
    }

    points_.clear();
    for (std::size_t i = 0; i < kRingPoints; ++i) {
        const float angle = spin_ + 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kRingPoints);
        const float r = baseRadius + values[i] * wobble;
        points_.push_back({cx + std::cos(angle) * r, cy + std::sin(angle) * r});
    }
    ring_.setPoints(points_, true);
}

void FeedbackTunnel::render() {
    if (!history_[0].texture()) return;

    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const RenderTarget& previous = history_[current_];
    const RenderTarget& next = history_[1 - current_];

    // Per-frame amounts are given for 60 fps and scaled by real frame time,
    // so the tunnel moves at the same speed on any monitor.
    const float frames = dt_ * 60.0f;
    const float zoom = std::pow(1.022f + 0.04f * beatPulse_ + 0.012f * bass_, frames);
    const float angle = (0.004f + 0.01f * mid_) * frames;
    const float fade = std::pow(0.925f, frames);
    const float hueShift = 0.012f * frames;

    buildRing();
    const float energy = std::min(level_ * 3.0f, 1.0f);
    const float scale = h / 720.0f;
    float color[3];
    hsvToRgb(time_ * 0.05f, 0.7f, 1.0f, color);

    {
        FramebufferScope scope;  // restores the caller's target when done

        next.bind();
        glDisable(GL_BLEND);
        warp_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, previous.texture());
        glUniform1i(warp_.uniform("uPrevious"), 0);
        glUniform2f(warp_.uniform("uAspect"), w / h, 1.0f);
        glUniform1f(warp_.uniform("uZoom"), zoom);
        glUniform1f(warp_.uniform("uAngle"), angle);
        glUniform1f(warp_.uniform("uFade"), fade);
        glUniform1f(warp_.uniform("uHueShift"), hueShift);
        drawFullscreen();

        // Only a dim copy of the ring goes into the history: it is what the
        // trails are made of, and it adds up over many frames.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        ring_.draw(w, h, (6.0f + 8.0f * energy) * scale, 1.5f, color, 0.10f + 0.12f * energy + 0.15f * beatPulse_);
        glDisable(GL_BLEND);
    }

    // Trails first, added onto the background the caller already cleared...
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    present_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, next.texture());
    glUniform1i(present_.uniform("uImage"), 0);
    drawFullscreen();

    // ...then the current ring on top, crisp and bright.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    ring_.draw(w, h, (10.0f + 14.0f * energy) * scale, 2.5f, color, 0.25f + 0.35f * energy + 0.3f * beatPulse_);
    ring_.draw(w, h, 2.0f * scale, 0.6f, color, 0.9f);
    glDisable(GL_BLEND);

    current_ = 1 - current_;
}
