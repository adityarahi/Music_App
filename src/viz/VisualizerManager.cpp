#include "viz/VisualizerManager.h"

#include "render/Fullscreen.h"

#include <algorithm>
#include <cstdio>

namespace {

constexpr float kFadeSeconds = 1.5f;

constexpr const char* kBlendFragmentSrc = R"(#version 330 core
in vec2 vUv;

uniform sampler2D uFrom;
uniform sampler2D uTo;
uniform float uMix;

out vec4 fragColor;

void main() {
    float t = smoothstep(0.0, 1.0, uMix);
    fragColor = vec4(mix(texture(uFrom, vUv).rgb, texture(uTo, vUv).rgb, t), 1.0);
}
)";

}  // namespace

void VisualizerManager::add(std::unique_ptr<Visualizer> visualizer) {
    visualizers_.push_back(std::move(visualizer));
}

bool VisualizerManager::init(int width, int height) {
    if (visualizers_.empty()) return false;
    if (!blend_.build(kFullscreenVertexSrc, kBlendFragmentSrc)) return false;

    for (auto& viz : visualizers_) {
        if (!viz->init(width, height)) {
            std::fprintf(stderr, "Visualizer '%s' failed to initialise.\n", viz->name());
            return false;
        }
    }
    resize(width, height);
    return true;
}

void VisualizerManager::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;  // minimised
    width_ = width;
    height_ = height;
    fromTarget_.create(width, height);
    toTarget_.create(width, height);
    for (auto& viz : visualizers_) viz->resize(width, height);
}

void VisualizerManager::clear() {
    visualizers_.clear();
    fromTarget_.destroy();
    toTarget_.destroy();
    blend_ = Shader{};
    active_ = fadingOut_ = 0;
    fade_ = 1.0f;
}

void VisualizerManager::next() { switchTo((active_ + 1) % visualizers_.size()); }

void VisualizerManager::previous() {
    switchTo((active_ + visualizers_.size() - 1) % visualizers_.size());
}

void VisualizerManager::switchTo(std::size_t index) {
    if (index == active_) return;
    fadingOut_ = active_;  // pressing again mid-fade fades from what was being faded to
    active_ = index;
    fade_ = 0.0f;
}

void VisualizerManager::update(const AudioFeatures& features, float dt) {
    level_ = features.level;
    beatPulse_ = features.beatPulse;

    if (fade_ < 1.0f) {
        fade_ = std::min(1.0f, fade_ + dt / kFadeSeconds);
        visualizers_[fadingOut_]->update(features, dt);
    }
    visualizers_[active_]->update(features, dt);
}

void VisualizerManager::renderOne(Visualizer& visualizer) {
    // Background breathes faintly with the music and flickers on beats.
    const float glow = std::min(level_ * 0.25f, 0.08f) + 0.03f * beatPulse_;
    glClearColor(0.01f + glow * 0.4f, 0.01f + glow * 0.3f, 0.03f + glow, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    visualizer.render();
}

void VisualizerManager::render() {
    glViewport(0, 0, width_, height_);

    if (fade_ >= 1.0f || !fromTarget_.texture()) {
        renderOne(*visualizers_[active_]);
        return;
    }

    {
        FramebufferScope scope;
        fromTarget_.bind();
        renderOne(*visualizers_[fadingOut_]);
        toTarget_.bind();
        renderOne(*visualizers_[active_]);
    }

    glDisable(GL_BLEND);
    blend_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fromTarget_.texture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, toTarget_.texture());
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(blend_.uniform("uFrom"), 0);
    glUniform1i(blend_.uniform("uTo"), 1);
    glUniform1f(blend_.uniform("uMix"), fade_);
    drawFullscreen();
}
