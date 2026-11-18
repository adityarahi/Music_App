#pragma once

#include "render/Shader.h"

#include <cstddef>
#include <vector>

struct Vec2 {
    float x, y;
};

// A thick, soft-edged polyline, drawn with additive blending so that a wide
// faint pass plus a thin bright pass read as a glowing line.
//
// setPoints() uploads the geometry once; draw() can then be called several
// times with different widths. Core-profile GL can't draw wide lines itself,
// so each point becomes two vertices that the vertex shader pushes apart.
class GlowLine {
public:
    ~GlowLine();

    bool init();

    // Points are in pixels of the target being drawn to. `closed` joins the
    // last point back to the first.
    void setPoints(const std::vector<Vec2>& points, bool closed);

    // Expects additive blending to be enabled by the caller.
    void draw(float targetWidth, float targetHeight, float halfWidth, float falloff, const float* rgb, float alpha);

private:
    struct Vertex {
        float x, y;     // point on the line, pixels
        float nx, ny;   // unit normal
        float side;     // -1 / +1
    };

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    std::vector<Vertex> vertices_;
};
