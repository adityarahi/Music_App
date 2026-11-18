#pragma once

#include "render/ColorMesh.h"
#include "viz/PeakCaps.h"
#include "viz/Visualizer.h"

#include <array>

// Classic spectrum analyzer: one glowing bar per frequency band with peak caps
// that hang briefly and then fall, mirrored as a faded reflection underneath.
class SpectrumBars final : public Visualizer {
public:
    const char* name() const override { return "Spectrum Bars"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    static constexpr std::size_t kBars = AudioFeatures::kBandCount;

    ColorMesh mesh_;

    int width_ = 1;
    int height_ = 1;

    std::array<float, kBars> heights_{};
    PeakCaps<kBars> peaks_;
    float beatPulse_ = 0.0f;
    float time_ = 0.0f;
};
