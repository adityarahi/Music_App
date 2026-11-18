#pragma once

#include "render/GlowLine.h"
#include "render/RenderTarget.h"
#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>
#include <vector>

// A glowing ring traced by the waveform, drawn over a zoomed, rotated,
// fading copy of the previous frame. The trails stream outward like a
// tunnel; beats kick the zoom and brighten the ring.
class FeedbackTunnel final : public Visualizer {
public:
    const char* name() const override { return "Feedback Tunnel"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    void buildRing();

    // history_[current_] holds the last frame; the other one is drawn into next.
    std::array<RenderTarget, 2> history_;
    int current_ = 0;

    Shader warp_;
    Shader present_;
    GlowLine ring_;
    std::vector<Vec2> points_;

    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kWaveformSize> mono_{};
    float bass_ = 0.0f;
    float mid_ = 0.0f;
    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
    float time_ = 0.0f;
    float dt_ = 0.0f;
    float spin_ = 0.0f;   // ring rotation, radians
};
