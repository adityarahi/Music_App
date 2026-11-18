#pragma once

#include "render/GlowLine.h"
#include "render/PointSprites.h"
#include "render/RenderTarget.h"
#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>
#include <random>
#include <vector>

// A glowing ring traced by the waveform, drawn over a zoomed, rotated,
// fading copy of the previous frame, so trails stream outward like a tunnel.
//
// The tunnel moves through "scenes": every 12-20 s it picks a new mix of
// optional features and fades them in or out over a couple of seconds:
//   rainbow    - colours run around the ring
//   stereo     - separate left / right channel rings spinning opposite ways
//   shockwaves - each beat fires an expanding ring and a burst of sparks
//   winding    - the vanishing point drifts and the walls swirl and ripple
//   flower     - the ring's outline is shaped by the frequency bands
class FeedbackTunnel final : public Visualizer {
public:
    FeedbackTunnel();

    const char* name() const override { return "Feedback Tunnel"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    struct Look {
        float rainbow = 0.0f;
        float stereo = 0.0f;
        float shockwaves = 0.0f;
        float winding = 0.0f;
        float flower = 0.0f;
    };

    struct Shockwave {
        float cx, cy;     // pixels
        float radius;     // pixels
        float speed;      // pixels per second
        float life;       // 1 -> 0
        float hue;
    };

    struct Spark {
        float x, y, vx, vy;   // pixels, pixels per second
        float life, maxLife;  // seconds
        float size, hue;
    };

    static constexpr std::size_t kRingPoints = 256;

    // Shapes are drawn twice: dimly into the feedback history (becoming
    // trails), then crisply onto the screen on top of the trails.
    enum class Layer { History, Screen };

    Look pickScene();
    void updateScene(float dt);
    void spawnBeatEffects();
    void updateParticles(float dt);

    float ringRadius() const;
    Vec2 ringCenter() const;
    void buildRing(GlowLine& line, const float* samples, float angleOffset, float radiusScale, float hueOffset);
    void drawRings(Layer layer);
    void drawShockwaves(float alphaScale);
    void drawSparks(float alphaScale);

    // history_[current_] holds the last frame; the other one is drawn into next.
    std::array<RenderTarget, 2> history_;
    int current_ = 0;

    Shader warp_;
    Shader present_;
    GlowLine monoRing_, leftRing_, rightRing_, shockRing_;
    PointSprites sparkSprites_;
    std::vector<Vec2> points_;
    std::vector<Rgb> colors_;
    std::vector<PointSprites::Sprite> sprites_;

    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kWaveformSize> left_{};
    std::array<float, AudioFeatures::kWaveformSize> right_{};
    std::array<float, AudioFeatures::kWaveformSize> mono_{};
    std::array<float, AudioFeatures::kBandCount> bands_{};
    float bass_ = 0.0f;
    float mid_ = 0.0f;
    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
    float time_ = 0.0f;
    float dt_ = 0.0f;
    float spin_ = 0.0f;       // ring rotation, radians
    float windPhase_ = 0.0f;  // drives the drifting vanishing point

    Look look_;        // current, eased
    Look target_;      // what the current scene asks for
    float sceneTimer_ = 0.0f;
    std::mt19937 rng_;

    std::vector<Shockwave> shockwaves_;
    std::vector<Spark> sparks_;
};
