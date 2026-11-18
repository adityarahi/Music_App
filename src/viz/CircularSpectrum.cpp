#include "viz/CircularSpectrum.h"

#include "render/Color.h"
#include "viz/WaveLoop.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

}  // namespace

bool CircularSpectrum::init(int width, int height) {
    if (!mesh_.init() || !waveRing_.init()) return false;
    points_.reserve(kLoopPoints);
    resize(width, height);
    return true;
}

void CircularSpectrum::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    cx_ = static_cast<float>(width_) * 0.5f;
    cy_ = static_cast<float>(height_) * 0.5f;
}

void CircularSpectrum::update(const AudioFeatures& features, float dt) {
    bands_ = features.bands;
    peaks_.update(bands_, dt);
    for (std::size_t i = 0; i < mono_.size(); ++i)
        mono_[i] = 0.5f * (features.waveformLeft[i] + features.waveformRight[i]);
    bass_ = features.bass;
    level_ = features.level;
    beatPulse_ = features.beatPulse;
    time_ += dt;
    spin_ += dt * 0.08f;
}

// A bar is a quad pointing straight out from the centre along `angle`.
void CircularSpectrum::addBar(float angle, float innerRadius, float outerRadius, float halfWidth, Rgba inner, Rgba outer) {
    const float dx = std::cos(angle), dy = std::sin(angle);   // outward
    const float px = -dy * halfWidth, py = dx * halfWidth;    // sideways
    const float ix = cx_ + dx * innerRadius, iy = cy_ + dy * innerRadius;
    const float ox = cx_ + dx * outerRadius, oy = cy_ + dy * outerRadius;
    const float xy[8] = {ix - px, iy - py, ix + px, iy + py, ox + px, oy + py, ox - px, oy - py};
    mesh_.addQuad(xy, inner, inner, outer, outer);
}

void CircularSpectrum::render() {
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float scale = h / 720.0f;

    const float radius = h * (0.17f + 0.025f * bass_ + 0.02f * beatPulse_);
    const float gap = 6.0f * scale;
    const float maxLength = h * 0.26f;
    const float halfWidth = kPi * radius / static_cast<float>(kBars) * 0.62f;
    const float flash = 1.0f + 0.35f * beatPulse_;
    const float baseHue = time_ * 0.02f;

    mesh_.clear();

    // Soft core: a disc that glows brighter with the bass, fading to nothing at its rim.
    {
        const Rgb core = hsv(baseHue + 0.55f, 0.6f, 1.0f);
        const Rgba centre{core.r, core.g, core.b, 0.10f + 0.25f * bass_ + 0.15f * beatPulse_};
        const Rgba rim{core.r, core.g, core.b, 0.0f};
        constexpr int kSegments = 64;
        for (int i = 0; i < kSegments; ++i) {
            const float a0 = 2.0f * kPi * static_cast<float>(i) / kSegments;
            const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / kSegments;
            mesh_.addTriangle(cx_, cy_, centre, cx_ + std::cos(a0) * radius, cy_ + std::sin(a0) * radius, rim,
                              cx_ + std::cos(a1) * radius, cy_ + std::sin(a1) * radius, rim);
        }
    }

    for (std::size_t k = 0; k < kBars; ++k) {
        // Bars run clockwise from the top; the second half mirrors the first,
        // so bass sits at the top on both sides and treble meets at the bottom.
        const std::size_t band = k < kBands ? k : kBars - 1 - k;
        const float angle = kPi * 0.5f - 2.0f * kPi * (static_cast<float>(k) + 0.5f) / static_cast<float>(kBars) + spin_;
        const float t = static_cast<float>(band) / static_cast<float>(kBands - 1);
        const float value = bands_[band];

        const Rgb deep = hsv(baseHue + t * 0.5f, 0.95f, 0.45f);
        const Rgb bright = hsv(baseHue + t * 0.5f + 0.06f, 0.55f, std::min(1.0f, 0.95f * flash));

        const float inner = radius + gap;
        const float outer = inner + std::max(value * maxLength, 2.0f * scale);
        addBar(angle, inner, outer, halfWidth, Rgba{deep.r, deep.g, deep.b, 1.0f}, Rgba{bright.r, bright.g, bright.b, 1.0f});

        // Halo, a little wider and longer, fading outward.
        addBar(angle, inner, outer + 8.0f * scale, halfWidth * 1.8f,
               Rgba{bright.r, bright.g, bright.b, 0.10f + 0.2f * value * flash}, Rgba{bright.r, bright.g, bright.b, 0.0f});

        // Reflection pointing inward, fading toward the centre.
        const float reflection = (outer - inner) * 0.3f;
        addBar(angle, radius - reflection, radius - 1.0f, halfWidth,
               Rgba{deep.r, deep.g, deep.b, 0.0f}, Rgba{bright.r, bright.g, bright.b, 0.25f});

        // Peak cap.
        const float cap = inner + peaks_[band] * maxLength + 3.0f * scale;
        addBar(angle, cap, cap + 4.0f * scale, halfWidth, Rgba{1.0f, 1.0f, 1.0f, 0.9f}, Rgba{bright.r, bright.g, bright.b, 0.9f});
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    mesh_.draw(w, h);

    // Waveform loop inside the ring.
    const auto values = waveLoop<kLoopPoints>(mono_.data(), mono_.size(), 2);
    points_.clear();
    for (std::size_t i = 0; i < kLoopPoints; ++i) {
        const float a = -spin_ * 2.0f + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kLoopPoints);
        const float r = radius * (0.62f + 0.35f * values[i]);
        points_.push_back({cx_ + std::cos(a) * r, cy_ + std::sin(a) * r});
    }
    waveRing_.setPoints(points_, true);

    const float energy = std::min(level_ * 3.0f, 1.0f);
    float color[3];
    hsvToRgb(baseHue + 0.5f, 0.35f, 1.0f, color);
    waveRing_.draw(w, h, (7.0f + 8.0f * energy) * scale, 2.5f, color, 0.3f + 0.4f * energy);
    waveRing_.draw(w, h, 1.6f * scale, 0.6f, color, 0.9f);

    glDisable(GL_BLEND);
}
