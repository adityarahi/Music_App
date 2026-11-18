#pragma once

#include "render/RenderTarget.h"
#include "render/Shader.h"
#include "viz/Visualizer.h"

#include <memory>
#include <vector>

// Owns every visualizer, draws the active one each frame, and cross-fades
// when switching. During a fade both effects keep animating, each rendered
// into its own offscreen target, then blended onto the screen.
class VisualizerManager {
public:
    void add(std::unique_ptr<Visualizer> visualizer);

    // Initialises everything added so far. Call with a current GL context.
    bool init(int width, int height);
    void resize(int width, int height);

    // Releases every visualizer's GL resources; call before the context is destroyed.
    void clear();

    void next();
    void previous();
    void select(std::size_t index);   // cross-fades to visualizer `index`

    // The visualizer being shown (or faded to).
    const Visualizer& current() const { return *visualizers_[active_]; }
    std::size_t currentIndex() const { return active_; }
    std::size_t count() const { return visualizers_.size(); }
    const Visualizer& at(std::size_t index) const { return *visualizers_[index]; }

    void update(const AudioFeatures& features, float dt);

    // Draws into the currently bound framebuffer (normally the window).
    void render();

private:
    void switchTo(std::size_t index);
    void renderOne(Visualizer& visualizer);

    std::vector<std::unique_ptr<Visualizer>> visualizers_;
    std::size_t active_ = 0;
    std::size_t fadingOut_ = 0;
    float fade_ = 1.0f;   // 0 = just switched, 1 = transition finished

    RenderTarget fromTarget_;
    RenderTarget toTarget_;
    Shader blend_;

    int width_ = 1;
    int height_ = 1;

    float level_ = 0.0f;
    float beatPulse_ = 0.0f;
};
