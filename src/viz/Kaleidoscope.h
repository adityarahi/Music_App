#pragma once

#include "render/GlowLine.h"
#include "render/PointSprites.h"
#include "render/RenderTarget.h"
#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>
#include <random>
#include <vector>

// Milkdrop-style kaleidoscope. Waveforms, spectrum "beads" and beat rings are
// painted into a feedback canvas that keeps flowing, swirling, zooming and
// shifting colour; the screen then shows one wedge of that canvas reflected
// around the centre like a mirror kaleidoscope, slowly turning.
class Kaleidoscope final : public Visualizer {
public:
    Kaleidoscope();

    const char* name() const override { return "Kaleidoscope"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    static constexpr float kSegments = 6.0f;   // mirror pairs around the circle

    void paintWaveform(GlowLine& line, const float* samples, float angle, float hue);
    void paintSpectrum();
    void paintBeatRing();

    std::array<RenderTarget, 2> history_;
    int current_ = 0;

    Shader warp_;
    Shader present_;
    GlowLine leftWave_, rightWave_, beatRing_;
    PointSprites beads_;
    std::vector<Vec2> points_;
    std::vector<PointSprites::Sprite> sprites_;

    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kWaveformSize> left_{};
    std::array<float, AudioFeatures::kWaveformSize> right_{};
    std::array<float, AudioFeatures::kBandCount> bands_{};
    float bass_ = 0.0f;
    float mid_ = 0.0f;
    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
    bool beat_ = false;
    float time_ = 0.0f;
    float dt_ = 0.0f;
    float rotation_ = 0.0f;   // of the mirror pattern on screen

    std::mt19937 rng_;
};
