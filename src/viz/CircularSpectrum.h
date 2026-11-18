#pragma once

#include "render/ColorMesh.h"
#include "render/GlowLine.h"
#include "viz/PeakCaps.h"
#include "viz/Visualizer.h"

#include <array>
#include <vector>

// The spectrum wrapped around a circle: bars radiate outward from a ring,
// mirrored left/right (bass at the top, treble meeting at the bottom), with
// peak caps and short inward reflections. Inside, a glowing waveform loop
// and a soft core that swell with the bass. The whole thing turns slowly.
class CircularSpectrum final : public Visualizer {
public:
    const char* name() const override { return "Circular Spectrum"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    static constexpr std::size_t kBands = AudioFeatures::kBandCount;
    static constexpr std::size_t kBars = kBands * 2;   // mirrored halves
    static constexpr std::size_t kLoopPoints = 192;

    void addBar(float angle, float innerRadius, float outerRadius, float halfWidth, Rgba inner, Rgba outer);

    ColorMesh mesh_;
    GlowLine waveRing_;
    std::vector<Vec2> points_;

    int width_ = 1;
    int height_ = 1;
    float cx_ = 0.0f;
    float cy_ = 0.0f;

    std::array<float, kBands> bands_{};
    PeakCaps<kBands> peaks_;
    std::array<float, AudioFeatures::kWaveformSize> mono_{};
    float bass_ = 0.0f;
    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
    float time_ = 0.0f;
    float spin_ = 0.0f;
};
