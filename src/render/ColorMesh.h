#pragma once

#include "render/Shader.h"

#include <vector>

struct Rgba {
    float r, g, b, a;
};

// Collects coloured triangles in pixel coordinates each frame and draws them
// in one call. Colours are interpolated across each triangle, which gives
// cheap gradients (bars fading from dark to bright, glows fading to nothing).
class ColorMesh {
public:
    ~ColorMesh();

    bool init();

    void clear() { vertices_.clear(); }
    void addTriangle(float x0, float y0, Rgba c0, float x1, float y1, Rgba c1, float x2, float y2, Rgba c2);
    // Corners in order around the quad (e.g. bottom-left, bottom-right, top-right, top-left).
    void addQuad(const float* xy, Rgba c0, Rgba c1, Rgba c2, Rgba c3);
    // Axis-aligned rectangle, one colour along the bottom edge and one along the top.
    void addRect(float x0, float y0, float x1, float y1, Rgba bottom, Rgba top);

    // Uses whatever blending the caller has set up.
    void draw(float targetWidth, float targetHeight);

private:
    struct Vertex {
        float x, y;
        float r, g, b, a;
    };

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::vector<Vertex> vertices_;
};
