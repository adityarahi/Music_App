#include "viz/Kaleidoscope.h"

#include "render/Color.h"
#include "render/Fullscreen.h"
#include "viz/WaveLoop.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// Moves the canvas like a slow liquid: a wavy flow field, gentle zoom and
// rotation, colour drift and fade, so painted shapes smear into ribbons.
constexpr const char* kWarpFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uPrevious;
uniform vec2 uAspect;
uniform float uZoom;
uniform float uAngle;
uniform float uFlow;
uniform float uTime;
uniform float uFade;
uniform float uHueShift;

out vec4 fragColor;

vec3 rotateHue(vec3 c, float angle) {
    const vec3 k = vec3(0.57735);
    float ca = cos(angle);
    return c * ca + cross(k, c) * sin(angle) + k * dot(k, c) * (1.0 - ca);
}

void main() {
    vec2 p = (vUv - 0.5) * uAspect;
    p += uFlow * vec2(sin(p.y * 7.0 + uTime * 0.9), cos(p.x * 6.0 - uTime * 0.7));
    float c = cos(uAngle), s = sin(uAngle);
    p = mat2(c, s, -s, c) * p / uZoom;
    vec2 uv = p / uAspect + 0.5;

    vec3 color = texture(uPrevious, uv).rgb;
    vec2 inside = step(vec2(0.0), uv) * step(uv, vec2(1.0));
    color *= inside.x * inside.y;

    color = max(rotateHue(color, uHueShift), 0.0) * uFade;
    fragColor = vec4(color, 1.0);
}
)";

// Folds the screen into mirrored wedges: every pixel's angle is mapped into
// the first half-wedge, alternately flipped, so one slice repeats around the circle.
constexpr const char* kPresentFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uImage;
uniform vec2 uAspect;
uniform float uSegments;
uniform float uRotation;

out vec4 fragColor;

void main() {
    vec2 p = (vUv - 0.5) * uAspect;
    float r = length(p);
    float seg = 6.28318 / uSegments;
    float a = mod(atan(p.y, p.x) + uRotation, seg);
    a = abs(a - seg * 0.5);                  // mirror: 0..seg/2
    vec2 q = r * vec2(cos(a), sin(a));
    vec2 uv = q / uAspect + 0.5;

    vec3 color = texture(uImage, uv).rgb;
    vec2 inside = step(vec2(0.0), uv) * step(uv, vec2(1.0));
    color *= inside.x * inside.y;

    color = 1.0 - exp(-color);               // soft clip
    color *= 1.0 - 0.45 * r * r;              // vignette
    fragColor = vec4(color, 1.0);
}
)";

}  // namespace

Kaleidoscope::Kaleidoscope() : rng_(std::random_device{}()) {}

bool Kaleidoscope::init(int width, int height) {
    if (!warp_.build(kFullscreenVertexSrc, kWarpFragmentSrc)) return false;
    if (!present_.build(kFullscreenVertexSrc, kPresentFragmentSrc)) return false;
    for (GlowLine* line : {&leftWave_, &rightWave_, &beatRing_})
        if (!line->init()) return false;
    if (!beads_.init()) return false;
    resize(width, height);
    return history_[0].texture() != 0;
}

void Kaleidoscope::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    for (auto& target : history_)
        target.create(width_, height_, GL_RGBA16F);
}

void Kaleidoscope::update(const AudioFeatures& features, float dt) {
    left_ = features.waveformLeft;
    right_ = features.waveformRight;
    bands_ = features.bands;
    bass_ = features.bass;
    mid_ = features.mid;
    level_ = features.level;
    beatPulse_ = features.beatPulse;
    beat_ = beat_ || features.beat;   // consumed by the next render
    time_ += dt;
    dt_ = dt;
    rotation_ += dt * (0.06f + 0.25f * mid_);
}

// A waveform drawn as a line running outward from the centre at `angle`.
void Kaleidoscope::paintWaveform(GlowLine& line, const float* samples, float angle, float hue) {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float dx = std::cos(angle), dy = std::sin(angle);
    const float amplitude = h * 0.08f;
    constexpr std::size_t kPoints = 96;

    // Smoothed so the canvas gets flowing ribbons rather than fuzz. Starts away
    // from the centre, where all the mirrored copies would otherwise pile up.
    const auto values = waveLoop<kPoints>(samples, AudioFeatures::kWaveformSize, 3);
    points_.clear();
    for (std::size_t i = 0; i < kPoints; ++i) {
        const float along = h * (0.10f + 0.40f * static_cast<float>(i) / static_cast<float>(kPoints - 1));
        const float v = values[i];
        points_.push_back({cx + dx * along - dy * v * amplitude, cy + dy * along + dx * v * amplitude});
    }
    line.setPoints(points_, false);

    const float energy = std::min(level_ * 3.0f, 1.0f);
    const float scale = h / 720.0f;
    float color[3];
    hsvToRgb(hue, 0.75f, 1.0f, color);
    line.draw(w, h, (5.0f + 6.0f * energy) * scale, 1.8f, color, 0.12f + 0.18f * energy);
    line.draw(w, h, 1.5f * scale, 0.6f, color, 0.45f);
}

// Spectrum as a string of glowing beads along the middle of the visible wedge.
void Kaleidoscope::paintSpectrum() {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float scale = h / 720.0f;
    const float angle = kPi / kSegments * 0.5f;   // centre of the half-wedge
    const float dx = std::cos(angle), dy = std::sin(angle);
    const float baseHue = time_ * 0.04f;

    sprites_.clear();
    for (std::size_t b = 0; b < bands_.size(); ++b) {
        const float t = static_cast<float>(b) / static_cast<float>(bands_.size() - 1);
        const float along = h * (0.06f + 0.42f * t);
        const float value = bands_[b];
        const Rgb c = hsv(baseHue + 0.5f + t * 0.3f, 0.6f, 1.0f);
        sprites_.push_back({w * 0.5f + dx * along, h * 0.5f + dy * along, (3.0f + 16.0f * value * value) * scale,
                            c.r, c.g, c.b, 0.1f + 0.35f * value});
    }
    beads_.draw(sprites_, w, h);
}

// On a beat, a ring at a random spot inside the visible wedge.
void Kaleidoscope::paintBeatRing() {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    const float angle = unit(rng_) * kPi / kSegments;
    const float along = h * (0.1f + 0.3f * unit(rng_));
    const float cx = w * 0.5f + std::cos(angle) * along;
    const float cy = h * 0.5f + std::sin(angle) * along;
    const float radius = h * (0.03f + 0.05f * bass_);

    constexpr std::size_t kPoints = 64;
    points_.clear();
    for (std::size_t i = 0; i < kPoints; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kPoints);
        points_.push_back({cx + std::cos(a) * radius, cy + std::sin(a) * radius});
    }
    beatRing_.setPoints(points_, true);

    float color[3];
    hsvToRgb(time_ * 0.04f + 0.5f, 0.4f, 1.0f, color);
    const float scale = h / 720.0f;
    beatRing_.draw(w, h, 10.0f * scale, 2.0f, color, 0.6f);
    beatRing_.draw(w, h, 2.0f * scale, 0.6f, color, 1.0f);
}

void Kaleidoscope::render() {
    if (!history_[0].texture()) return;

    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const RenderTarget& previous = history_[current_];
    const RenderTarget& next = history_[1 - current_];

    const float frames = dt_ * 60.0f;
    const float zoom = std::pow(1.006f + 0.02f * beatPulse_, frames);
    const float angle = (0.003f + 0.006f * mid_) * frames;
    const float flow = 0.0025f * (0.5f + bass_) * frames;
    const float fade = std::pow(0.93f, frames);
    const float hueShift = 0.006f * frames;

    {
        FramebufferScope scope;

        next.bind();
        glDisable(GL_BLEND);
        warp_.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, previous.texture());
        glUniform1i(warp_.uniform("uPrevious"), 0);
        glUniform2f(warp_.uniform("uAspect"), w / h, 1.0f);
        glUniform1f(warp_.uniform("uZoom"), zoom);
        glUniform1f(warp_.uniform("uAngle"), angle);
        glUniform1f(warp_.uniform("uFlow"), flow);
        glUniform1f(warp_.uniform("uTime"), time_);
        glUniform1f(warp_.uniform("uFade"), fade);
        glUniform1f(warp_.uniform("uHueShift"), hueShift);
        drawFullscreen();

        // Paint new shapes inside the wedge the screen will mirror.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        const float wedge = kPi / kSegments;   // half of one mirror segment
        const float baseHue = time_ * 0.04f;
        paintWaveform(leftWave_, left_.data(), wedge * (0.25f + 0.12f * std::sin(time_ * 0.7f)), baseHue);
        paintWaveform(rightWave_, right_.data(), wedge * (0.75f + 0.12f * std::sin(time_ * 0.53f)), baseHue + 0.3f);
        paintSpectrum();
        if (beat_) paintBeatRing();
        beat_ = false;
        glDisable(GL_BLEND);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    present_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, next.texture());
    glUniform1i(present_.uniform("uImage"), 0);
    glUniform2f(present_.uniform("uAspect"), w / h, 1.0f);
    glUniform1f(present_.uniform("uSegments"), kSegments);
    glUniform1f(present_.uniform("uRotation"), rotation_);
    drawFullscreen();
    glDisable(GL_BLEND);

    current_ = 1 - current_;
}
