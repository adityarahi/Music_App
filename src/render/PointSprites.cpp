#include "render/PointSprites.h"

#include <cstddef>

namespace {

constexpr const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4 aColor;

uniform vec2 uResolution;
uniform float uMaxSize;

out vec4 vColor;

void main() {
    gl_Position = vec4(aPos / uResolution * 2.0 - 1.0, 0.0, 1.0);
    gl_PointSize = min(aSize, uMaxSize);
    vColor = aColor;
}
)";

constexpr const char* kFragmentSrc = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;

void main() {
    float d = length(gl_PointCoord - 0.5) * 2.0;   // 0 at centre, 1 at edge
    float glow = pow(max(1.0 - d, 0.0), 1.6);
    fragColor = vec4(vColor.rgb, vColor.a * glow);
}
)";

}  // namespace

PointSprites::~PointSprites() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool PointSprites::init() {
    if (!shader_.build(kVertexSrc, kFragmentSrc)) return false;

    GLfloat range[2] = {1.0f, 1.0f};
    glGetFloatv(GL_POINT_SIZE_RANGE, range);
    maxSize_ = range[1];

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const auto stride = static_cast<GLsizei>(sizeof(Sprite));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Sprite, x)));
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Sprite, size)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Sprite, r)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return true;
}

void PointSprites::draw(const std::vector<Sprite>& sprites, float targetWidth, float targetHeight) {
    if (sprites.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sprites.size() * sizeof(Sprite)), sprites.data(), GL_STREAM_DRAW);

    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), targetWidth, targetHeight);
    glUniform1f(shader_.uniform("uMaxSize"), maxSize_);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sprites.size()));
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
}
