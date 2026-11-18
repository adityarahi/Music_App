#pragma once

#include <string>
#include <utility>

class AudioEngine;
class Ui;
class VisualizerManager;

// Developer aid, enabled by setting MUSIC_APP_TOUR=<directory>.
// Seeks into the track, captures the on-screen controls with the playlist
// open, then hides them and shows every visualizer in turn, saving a
// screenshot of each one (plus one mid cross-fade) as a .ppm image, then
// asks the app to quit. Lets visuals be checked without watching the window.
class DevTour {
public:
    explicit DevTour(std::string directory) : directory_(std::move(directory)) {}

    // Call after the frame is rendered, before swapping buffers.
    // Returns true once the tour is finished.
    bool afterRender(float dt, AudioEngine& audio, VisualizerManager& visualizers, Ui& ui, int width, int height);

private:
    void capture(const std::string& name, int width, int height) const;

    std::string directory_;
    float clock_ = 0.0f;
    int shown_ = 0;           // visualizers captured so far
    bool started_ = false;
    bool uiCaptured_ = false;
    bool fadeCaptured_ = false;
};
