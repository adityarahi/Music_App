#include "DevTour.h"

#include "audio/AudioEngine.h"
#include "ui/Ui.h"
#include "viz/VisualizerManager.h"

#include <glad/gl.h>

#include <cctype>
#include <cstdio>
#include <vector>

namespace {

constexpr float kSettleSeconds = 3.5f;     // let feedback trails and smoothing build up
constexpr float kMidFadeSeconds = 0.75f;   // half of the 1.5 s cross-fade
constexpr double kStartAt = 60.0;          // skip quiet intros
constexpr float kUiSeconds = 1.5f;         // let the UI fade in before the controls shot

std::string slug(const char* name) {
    std::string out;
    for (const char* c = name; *c; ++c)
        out += std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '_';
    return out;
}

}  // namespace

bool DevTour::afterRender(float dt, AudioEngine& audio, VisualizerManager& visualizers, Ui& ui, int width, int height) {
    if (!started_) {
        started_ = true;
        if (audio.duration() > kStartAt + 30.0) audio.seek(kStartAt);
        ui.setPlaylistOpen(true);
        ui.setSettingsOpen(true);
        std::printf("Tour: capturing %zu visualizers into %s\n", visualizers.count(), directory_.c_str());
    }

    clock_ += dt;

    // First shot: the controls and playlist; after that, hide them for the visualizer shots.
    if (!uiCaptured_) {
        if (clock_ < kUiSeconds) return false;
        capture("0_controls", width, height);
        uiCaptured_ = true;
        ui.setPlaylistOpen(false);
        ui.setSettingsOpen(false);
        ui.hideUntilMouseMoves();
        clock_ = 0.0f;
        return false;
    }

    if (shown_ > 0 && !fadeCaptured_ && clock_ >= kMidFadeSeconds) {
        capture(std::to_string(shown_) + "_crossfade", width, height);
        fadeCaptured_ = true;
    }

    if (clock_ >= kSettleSeconds) {
        capture(std::to_string(shown_ + 1) + "_" + slug(visualizers.current().name()), width, height);
        ++shown_;
        if (static_cast<std::size_t>(shown_) >= visualizers.count()) return true;
        visualizers.next();
        clock_ = 0.0f;
        fadeCaptured_ = shown_ > 1;  // one cross-fade shot is enough
    }
    return false;
}

void DevTour::capture(const std::string& name, int width, int height) const {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    const std::string path = directory_ + "/" + name + ".ppm";
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        std::fprintf(stderr, "Tour: cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    // OpenGL rows start at the bottom; image files start at the top.
    for (int y = height - 1; y >= 0; --y)
        std::fwrite(pixels.data() + static_cast<std::size_t>(y) * width * 3, 1, static_cast<std::size_t>(width) * 3, file);
    std::fclose(file);
    std::printf("Tour: saved %s\n", path.c_str());
}
