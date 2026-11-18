#include "viz/SpectrumBars.h"

#include "render/Color.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

bool SpectrumBars::init(int width, int height) {
    if (!mesh_.init()) return false;
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
    peaks_.update(heights_, dt);
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

    mesh_.clear();
    for (std::size_t i = 0; i < kBars; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kBars - 1);
        const float x0 = margin + static_cast<float>(i) * slot + (slot - barWidth) * 0.5f;
        const float x1 = x0 + barWidth;
        const float barTop = baseline + std::max(heights_[i] * maxHeight, 2.0f * scale);
        const float capY = baseline + peaks_[i] * maxHeight + capGap;

        // Bass on the left warm, treble on the right cool; the whole palette drifts.
        const Rgb deep = hsv(baseHue + t * 0.45f, 0.95f, 0.45f);
        const Rgb bright = hsv(baseHue + t * 0.45f + 0.06f, 0.55f, std::min(1.0f, 0.95f * flash));

        const Rgba bottom{deep.r, deep.g, deep.b, 1.0f};
        const Rgba top{bright.r, bright.g, bright.b, 1.0f};
        const Rgba glow{bright.r, bright.g, bright.b, 0.12f + 0.25f * heights_[i] * flash};
        const Rgba clear{bright.r, bright.g, bright.b, 0.0f};

        // Soft halo around the bar, then the bar itself (additive, so they blend into light).
        mesh_.addRect(x0 - glowPad, baseline, x1 + glowPad, barTop + glowPad, glow, clear);
        mesh_.addRect(x0, baseline, x1, barTop, bottom, top);

        // Reflection: a shorter, fading copy below the baseline.
        const float reflection = (barTop - baseline) * 0.35f;
        mesh_.addRect(x0, baseline - reflection, x1, baseline,
                      Rgba{deep.r, deep.g, deep.b, 0.0f}, Rgba{bright.r, bright.g, bright.b, 0.22f});

        mesh_.addRect(x0, capY, x1, capY + capHeight, Rgba{1.0f, 1.0f, 1.0f, 0.9f}, Rgba{bright.r, bright.g, bright.b, 0.9f});
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    mesh_.draw(w, h);
    glDisable(GL_BLEND);
}
