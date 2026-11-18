#pragma once

#include "render/GlowLine.h"
#include "viz/Visualizer.h"

#include <array>
#include <vector>

// Two glowing waveform traces (left channel above, right below) whose colour
// drifts over time and brightens with loudness.
class Oscilloscope final : public Visualizer {
public:
    const char* name() const override { return "Oscilloscope"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    void buildTrace(const float* samples, std::size_t count, float centerY, float amplitude);

    GlowLine left_;
    GlowLine right_;
    std::vector<Vec2> points_;

    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kWaveformSize> leftSamples_{};
    std::array<float, AudioFeatures::kWaveformSize> rightSamples_{};
    float level_ = 0.0f;
    float time_ = 0.0f;
};
