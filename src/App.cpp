#include "App.h"

#include "viz/Oscilloscope.h"
#include "viz/SpectrumBars.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>

namespace {

constexpr const char* kAppName = "Music App";

std::string fileNameOf(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string formatTime(double seconds) {
    const int total = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

App* fromWindow(GLFWwindow* w) { return static_cast<App*>(glfwGetWindowUserPointer(w)); }

}  // namespace

App::App() = default;

App::~App() { shutdown(); }

int App::run(const std::string& initialFile) {
    if (!init()) return 1;
    if (!initialFile.empty()) openFile(initialFile);

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - last, 0.1));  // clamp after stalls (e.g. window drag)
        last = now;

        frame(dt);
        glfwSwapBuffers(window_);
    }

    shutdown();
    return 0;
}

bool App::init() {
    glfwSetErrorCallback([](int code, const char* msg) { std::fprintf(stderr, "GLFW error %d: %s\n", code, msg); });
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // required on macOS, harmless elsewhere
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(1280, 720, kAppName, nullptr, nullptr);
    if (!window_) {
        std::fprintf(stderr, "Could not create an OpenGL 3.3 window.\n");
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // vsync

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to load OpenGL functions.\n");
        return false;
    }
    std::printf("OpenGL: %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int mods) { fromWindow(w)->onKey(key, action, mods); });
    glfwSetDropCallback(window_, [](GLFWwindow* w, int count, const char** paths) { fromWindow(w)->onDrop(count, paths); });
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) { fromWindow(w)->onFramebufferResize(width, height); });

    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);

    if (!audio_.init()) return false;

    visualizers_.push_back(std::make_unique<SpectrumBars>());
    visualizers_.push_back(std::make_unique<Oscilloscope>());
    for (auto& viz : visualizers_) {
        if (!viz->init(fbWidth_, fbHeight_)) {
            std::fprintf(stderr, "Visualizer '%s' failed to initialise.\n", viz->name());
            return false;
        }
    }

    updateTitle();
    return true;
}

void App::shutdown() {
    audio_.shutdown();
    if (window_) {
        visualizers_.clear();  // GL objects die while the context is still alive
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

void App::frame(float dt) {
    analyzer_.update(audio_.samples(), audio_.sampleRate(), dt);
    const AudioFeatures& features = analyzer_.features();

    // Background breathes faintly with the music and flickers on beats.
    const float glow = std::min(features.level * 0.25f, 0.08f) + 0.03f * features.beatPulse;
    glViewport(0, 0, fbWidth_, fbHeight_);
    glClearColor(0.01f + glow * 0.4f, 0.01f + glow * 0.3f, 0.03f + glow, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Visualizer& viz = *visualizers_[activeViz_];
    viz.update(features, dt);
    viz.render();

    titleTimer_ -= dt;
    if (titleTimer_ <= 0.0) updateTitle();
}

void App::updateTitle() {
    titleTimer_ = 0.25;
    std::string title = kAppName;
    if (audio_.hasTrack()) {
        title = trackName_ + "  [" + formatTime(audio_.position()) + " / " + formatTime(audio_.duration()) + "]";
        if (!audio_.isPlaying()) title += "  (paused)";
        title += "  vol " + std::to_string(static_cast<int>(audio_.volume() * 100.0f + 0.5f)) + "%";
        title += "  | " + std::string(visualizers_[activeViz_]->name()) + "  - " + kAppName;
    } else {
        title += "  - drop an MP3 file here";
    }
    glfwSetWindowTitle(window_, title.c_str());
}

void App::openFile(const std::string& path) {
    if (audio_.load(path)) {
        trackName_ = fileNameOf(path);
        std::printf("Playing: %s\n", path.c_str());
    } else {
        trackName_.clear();
    }
    updateTitle();
}

void App::onKey(int key, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    const double seekStep = (mods & GLFW_MOD_SHIFT) ? 30.0 : 5.0;

    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
        case GLFW_KEY_SPACE:  if (action == GLFW_PRESS) audio_.togglePause(); break;
        case GLFW_KEY_RIGHT:  audio_.seek(audio_.position() + seekStep); break;
        case GLFW_KEY_LEFT:   audio_.seek(audio_.position() - seekStep); break;
        case GLFW_KEY_UP:     audio_.setVolume(audio_.volume() + 0.05f); break;
        case GLFW_KEY_DOWN:   audio_.setVolume(audio_.volume() - 0.05f); break;
        case GLFW_KEY_V:
            if (action == GLFW_PRESS) {
                activeViz_ = (activeViz_ + 1) % visualizers_.size();
                std::printf("Visualizer: %s\n", visualizers_[activeViz_]->name());
            }
            break;
        default: return;
    }
    updateTitle();
}

void App::onDrop(int count, const char** paths) {
    if (count > 0) openFile(paths[0]);  // playlist support comes later
}

void App::onFramebufferResize(int width, int height) {
    fbWidth_ = width;
    fbHeight_ = height;
    if (width == 0 || height == 0) return;  // minimised
    for (auto& viz : visualizers_) viz->resize(width, height);
}
