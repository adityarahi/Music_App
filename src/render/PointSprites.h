#pragma once

#include "render/Shader.h"

#include <vector>

// Soft round glowing dots (sparks, particles), drawn as GL point sprites.
class PointSprites {
public:
    struct Sprite {
        float x, y;      // pixels
        float size;      // diameter, pixels
        float r, g, b, a;
    };

    ~PointSprites();

    bool init();

    // Expects additive blending to be enabled by the caller.
    void draw(const std::vector<Sprite>& sprites, float targetWidth, float targetHeight);

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    float maxSize_ = 1.0f;
};
