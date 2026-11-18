#pragma once

#include "audio/AudioAnalyzer.h"
#include "audio/AudioEngine.h"
#include "viz/Visualizer.h"

#include <memory>
#include <string>
#include <vector>

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

    std::vector<std::unique_ptr<Visualizer>> visualizers_;
    std::size_t activeViz_ = 0;

    std::string trackName_;
    double titleTimer_ = 0.0;
};
