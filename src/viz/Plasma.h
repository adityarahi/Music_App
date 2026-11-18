#pragma once

#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>

// A full-screen flowing colour field computed entirely in a fragment shader.
// Bass warps it, the overall pace follows the music's energy, the spectrum
// bands shape its brightness radially, and beats flash it.
class Plasma final : public Visualizer {
public:
    const char* name() const override { return "Plasma"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    Shader shader_;
    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kBandCount> bands_{};
    float bass_ = 0.0f;
    float mid_ = 0.0f;
    float treble_ = 0.0f;
    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
    float phase_ = 0.0f;   // animation clock that runs faster when the music is busier
};
