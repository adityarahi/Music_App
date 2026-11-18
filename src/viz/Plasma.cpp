#include "viz/Plasma.h"

#include "render/Fullscreen.h"

#include <algorithm>

namespace {

constexpr const char* kFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform vec2 uResolution;
uniform float uPhase;
uniform float uBass;
uniform float uMid;
uniform float uTreble;
uniform float uLevel;
uniform float uBeat;
uniform float uBands[32];

out vec4 fragColor;

// Smooth cyclic palette: t in any range, returns a saturated colour.
vec3 palette(float t) {
    return 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));
}

void main() {
    vec2 uv = (vUv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);
    float t = uPhase;

    // Domain warp: bend the coordinates with slow waves before sampling the
    // pattern. Bass makes the bending stronger.
    vec2 p = uv * 3.0;
    float warp = 0.35 + 0.9 * uBass;
    p += warp * vec2(sin(p.y * 1.7 + t * 0.9), cos(p.x * 1.3 - t * 0.7));
    p += 0.5 * warp * vec2(sin(p.y * 2.9 - t * 1.3), cos(p.x * 2.3 + t * 1.1));

    float v = sin(p.x + t)
            + sin(p.y * 1.3 - t * 0.8)
            + sin((p.x + p.y) * 0.8 + t * 0.6)
            + sin(length(p) * 2.0 - t * 1.5);
    v *= 0.25;

    // Map angle around the centre to a spectrum band, mirrored left/right so
    // the image stays symmetric: bass at the bottom, treble at the top.
    // Interpolate between neighbouring bands so there are no hard spokes.
    float angle = atan(uv.x, -uv.y) / 3.14159;          // -1..1
    float position = clamp(abs(angle), 0.0, 1.0) * 31.0;
    int band = int(position);
    float energy = mix(uBands[band], uBands[min(band + 1, 31)], fract(position));

    vec3 color = palette(v * 0.8 + t * 0.04 + uMid * 0.25);
    color *= 0.25 + 0.75 * smoothstep(-0.3, 1.0, v + energy * 0.8);

    // Fine shimmering ridges driven by treble.
    color += uTreble * 0.25 * pow(abs(sin(v * 18.0 + t * 3.0)), 12.0);

    color *= 0.55 + 0.9 * min(uLevel * 3.0, 1.0) + 0.35 * uBeat;
    color *= 1.0 - 0.5 * dot(uv, uv);   // vignette

    fragColor = vec4(color, 1.0);
}
)";

}  // namespace

bool Plasma::init(int width, int height) {
    if (!shader_.build(kFullscreenVertexSrc, kFragmentSrc)) return false;
    resize(width, height);
    return true;
}

void Plasma::resize(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
}

void Plasma::update(const AudioFeatures& features, float dt) {
    bands_ = features.bands;
    bass_ = features.bass;
    mid_ = features.mid;
    treble_ = features.treble;
    level_ = features.level;
    beatPulse_ = features.beatPulse;
    phase_ += dt * (0.25f + 1.2f * mid_ + 0.6f * beatPulse_);
}

void Plasma::render() {
    shader_.use();
    glUniform2f(shader_.uniform("uResolution"), static_cast<float>(width_), static_cast<float>(height_));
    glUniform1f(shader_.uniform("uPhase"), phase_);
    glUniform1f(shader_.uniform("uBass"), bass_);
    glUniform1f(shader_.uniform("uMid"), mid_);
    glUniform1f(shader_.uniform("uTreble"), treble_);
    glUniform1f(shader_.uniform("uLevel"), level_);
    glUniform1f(shader_.uniform("uBeat"), beatPulse_);
    glUniform1fv(shader_.uniform("uBands"), static_cast<GLsizei>(bands_.size()), bands_.data());

    glDisable(GL_BLEND);
    drawFullscreen();
}
