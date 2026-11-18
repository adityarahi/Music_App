#pragma once

#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>
#include <vector>

// Classic spectrum analyzer: one glowing bar per frequency band with peak caps
// that hang briefly and then fall, mirrored as a faded reflection underneath.
class SpectrumBars final : public Visualizer {
public:
    ~SpectrumBars() override;

    const char* name() const override { return "Spectrum Bars"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    static constexpr std::size_t kBars = AudioFeatures::kBandCount;

    struct Vertex {
        float x, y;         // pixels
        float r, g, b, a;
    };

    struct Color {
        float r, g, b, a;
    };

    // Quad with one colour along the bottom edge and another along the top.
    void addQuad(float x0, float y0, float x1, float y1, Color bottom, Color top);

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    int width_ = 1;
    int height_ = 1;

    std::array<float, kBars> heights_{};
    std::array<float, kBars> peaks_{};
    std::array<float, kBars> peakVelocity_{};
    std::array<float, kBars> peakHold_{};
    float beatPulse_ = 0.0f;
    float time_ = 0.0f;

    std::vector<Vertex> vertices_;
};
