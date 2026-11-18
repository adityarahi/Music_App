#pragma once

#include "audio/AudioAnalyzer.h"
#include "audio/AudioEngine.h"
#include "DevTour.h"
#include "viz/VisualizerManager.h"

#include <memory>
#include <string>

struct GLFWwindow;

class App {
public:
    App();
    ~App();

    // `initialFile` (UTF-8, may be empty) starts playing right away.
    int run(const std::string& initialFile);

private:
    bool init();
    void shutdown();
    void frame(float dt);
    void updateTitle();
    void openFile(const std::string& path);

    void onKey(int key, int action, int mods);
    void onDrop(int count, const char** paths);
    void onFramebufferResize(int width, int height);

    GLFWwindow* window_ = nullptr;
    int fbWidth_ = 0;
    int fbHeight_ = 0;

    AudioEngine audio_;
    AudioAnalyzer analyzer_;

    VisualizerManager visualizers_;
    std::unique_ptr<DevTour> tour_;   // only when MUSIC_APP_TOUR is set

    std::string trackName_;
    double titleTimer_ = 0.0;
};
