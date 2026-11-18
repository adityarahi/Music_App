#pragma once

// Vertex shader for full-screen passes. Pair it with a fragment shader that
// reads `in vec2 vUv;` (0..1 across the target).
inline constexpr const char* kFullscreenVertexSrc = R"(#version 330 core
out vec2 vUv;

void main() {
    // One oversized triangle covering the screen; no vertex buffer needed.
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Draws the full-screen triangle with whatever shader is bound.
void drawFullscreen();
