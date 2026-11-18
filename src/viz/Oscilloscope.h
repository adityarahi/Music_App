#pragma once

#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <array>
#include <vector>

// Two glowing waveform traces (left channel above, right below) whose colour
// drifts over time and brightens with loudness.
class Oscilloscope final : public Visualizer {
public:
    ~Oscilloscope() override;

    const char* name() const override { return "Oscilloscope"; }
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void update(const AudioFeatures& features, float dt) override;
    void render() override;

private:
    // The vertex shader pushes each vertex `side * halfWidth` pixels along
    // `normal`, so one set of geometry serves both the glow and core passes.
    struct Vertex {
        float x, y;     // point on the trace, pixels
        float nx, ny;   // unit normal, pixels
        float side;     // -1 / +1
    };

    void appendTrace(const float* samples, std::size_t count, float centerY, float amplitude);
    void drawPass(float halfWidth, float falloff, const float* rgb, const float* rgb2, float alpha);

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    int width_ = 1;
    int height_ = 1;

    std::array<float, AudioFeatures::kWaveformSize> left_{};
    std::array<float, AudioFeatures::kWaveformSize> right_{};
    float level_ = 0.0f;
    float time_ = 0.0f;

    std::vector<Vertex> vertices_;
};
