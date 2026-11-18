#pragma once

#include "audio/AudioAnalyzer.h"
#include "audio/AudioEngine.h"
#include "DevTour.h"
#include "player/Player.h"
#include "ui/Ui.h"
#include "viz/VisualizerManager.h"

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

class App {
public:
    App();
    ~App();

    // `paths` (UTF-8 files or folders, may be empty) are queued and the first starts playing.
    int run(const std::vector<std::string>& paths);

private:
    bool init();
    void shutdown();
    void frame(float dt);
    void updateTitle();

    void onKey(int key, int action, int mods);
    void onDrop(int count, const char** paths);
    void onFramebufferResize(int width, int height);

    GLFWwindow* window_ = nullptr;
    int fbWidth_ = 0;
    int fbHeight_ = 0;

    AudioEngine audio_;
    AudioAnalyzer analyzer_;
    Player player_{audio_};

    VisualizerManager visualizers_;
    Ui ui_;
    std::unique_ptr<DevTour> tour_;   // only when MUSIC_APP_TOUR is set

    double titleTimer_ = 0.0;
};
