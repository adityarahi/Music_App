#include "render/ColorMesh.h"

#include <cstddef>

namespace {

constexpr const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform vec2 uResolution;

out vec4 vColor;

void main() {
    gl_Position = vec4(aPos / uResolution * 2.0 - 1.0, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr const char* kFragmentSrc = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;

void main() {
    fragColor = vColor;
}
)";

}  // namespace

ColorMesh::~ColorMesh() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool ColorMesh::init() {
    if (!shader_.build(kVertexSrc, kFragmentSrc)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, x)));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return true;
}

void ColorMesh::addTriangle(float x0, float y0, Rgba c0, float x1, float y1, Rgba c1, float x2, float y2, Rgba c2) {
    vertices_.push_back({x0, y0, c0.r, c0.g, c0.b, c0.a});
    vertices_.push_back({x1, y1, c1.r, c1.g, c1.b, c1.a});
    vertices_.push_back({x2, y2, c2.r, c2.g, c2.b, c2.a});
}

void ColorMesh::addQuad(const float* xy, Rgba c0, Rgba c1, Rgba c2, Rgba c3) {
    addTriangle(xy[0], xy[1], c0, xy[2], xy[3], c1, xy[4], xy[5], c2);
    addTriangle(xy[0], xy[1], c0, xy[4], xy[5], c2, xy[6], xy[7], c3);
}

void ColorMesh::addRect(float x0, float y0, float x1, float y1, Rgba bottom, Rgba top) {
    const float xy[8] = {x0, y0, x1, y0, x1, y1, x0, y1};
    addQuad(xy, bottom, bottom, top, top);
}

void ColorMesh::draw(float targetWidth, float targetHeight) {
    if (vertices_.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(), GL_STREAM_DRAW);

    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), targetWidth, targetHeight);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
}
