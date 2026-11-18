#pragma once

#include "audio/AudioAnalyzer.h"

// One visual effect. The app owns a list of these and renders the active one
// each frame; all GL calls happen on the render thread with a current context.
class Visualizer {
public:
    virtual ~Visualizer() = default;

    virtual const char* name() const = 0;

    // Create GL resources. Return false if something (e.g. a shader) failed.
    virtual bool init(int width, int height) = 0;
    virtual void resize(int width, int height) = 0;

    virtual void update(const AudioFeatures& features, float dt) = 0;
    virtual void render() = 0;
};
