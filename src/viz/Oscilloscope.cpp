#include "viz/Oscilloscope.h"

#include "render/Color.h"

#include <algorithm>

bool Oscilloscope::init(int width, int height) {
    if (!left_.init() || !right_.init()) return false;
    points_.reserve(AudioFeatures::kWaveformSize);
    resize(width, height);
    return true;
}

void Oscilloscope::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
}

void Oscilloscope::update(const AudioFeatures& features, float dt) {
    leftSamples_ = features.waveformLeft;
    rightSamples_ = features.waveformRight;
    level_ = features.level;
    time_ += dt;
}

void Oscilloscope::buildTrace(const float* samples, std::size_t count, float centerY, float amplitude) {
    const float step = static_cast<float>(width_) / static_cast<float>(count - 1);
    points_.clear();
    for (std::size_t i = 0; i < count; ++i)
        points_.push_back({static_cast<float>(i) * step, centerY + samples[i] * amplitude});
}

void Oscilloscope::render() {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float amplitude = h * 0.22f;
    const std::size_t n = leftSamples_.size();

    buildTrace(leftSamples_.data(), n, h * 0.70f, amplitude);
    left_.setPoints(points_, false);
    buildTrace(rightSamples_.data(), n, h * 0.30f, amplitude);
    right_.setPoints(points_, false);

    // Hue drifts slowly on its own and jumps ahead a little with loudness.
    const float energy = std::min(level_ * 3.0f, 1.0f);
    const float hue = time_ * 0.03f + energy * 0.08f;
    float colorL[3], colorR[3];
    hsvToRgb(hue, 0.75f, 1.0f, colorL);
    hsvToRgb(hue + 0.12f, 0.75f, 1.0f, colorR);

    const float scale = h / 720.0f;  // keep line widths proportional to window size
    const float glowWidth = (14.0f + 18.0f * energy) * scale;
    const float glowAlpha = 0.35f + 0.4f * energy;
    const float coreWidth = 2.2f * scale;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive: overlapping light adds up into glow

    left_.draw(w, h, glowWidth, 2.5f, colorL, glowAlpha);
    right_.draw(w, h, glowWidth, 2.5f, colorR, glowAlpha);
    left_.draw(w, h, coreWidth, 0.6f, colorL, 1.0f);
    right_.draw(w, h, coreWidth, 0.6f, colorR, 1.0f);

    glDisable(GL_BLEND);
}
