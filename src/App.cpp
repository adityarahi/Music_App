#include "App.h"

#include "viz/CircularSpectrum.h"
#include "viz/FeedbackTunnel.h"
#include "viz/Kaleidoscope.h"
#include "viz/Oscilloscope.h"
#include "viz/Plasma.h"
#include "viz/SpectrumBars.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* kAppName = "Music App";

std::string formatTime(double seconds) {
    const int total = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

App* fromWindow(GLFWwindow* w) { return static_cast<App*>(glfwGetWindowUserPointer(w)); }

// The monitor that contains most of the window, so full screen happens where the window is.
GLFWmonitor* monitorForWindow(GLFWwindow* window) {
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* best = glfwGetPrimaryMonitor();
    int bestArea = 0;
    for (int i = 0; i < count; ++i) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const int overlapW = std::min(wx + ww, mx + mode->width) - std::max(wx, mx);
        const int overlapH = std::min(wy + wh, my + mode->height) - std::max(wy, my);
        const int area = std::max(overlapW, 0) * std::max(overlapH, 0);
        if (area > bestArea) {
            bestArea = area;
            best = monitors[i];
        }
    }
    return best;
}

}  // namespace

App::App() = default;

App::~App() { shutdown(); }

int App::run(const std::vector<std::string>& paths) {
    if (!init()) return 1;
    if (!paths.empty()) player_.enqueue(paths);

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - last, 0.1));  // clamp after stalls (e.g. window drag)
        last = now;

        frame(dt);
        if (tour_ && tour_->afterRender(dt, audio_, visualizers_, ui_, fbWidth_, fbHeight_))
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        glfwSwapBuffers(window_);

        // Switched between frames so nothing is mid-draw when the size changes.
        if (ui_.takeFullscreenRequest()) toggleFullscreen();
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

    // Installed before the UI so ImGui chains to them rather than replacing them.
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int mods) { fromWindow(w)->onKey(key, action, mods); });
    glfwSetDropCallback(window_, [](GLFWwindow* w, int count, const char** paths) { fromWindow(w)->onDrop(count, paths); });
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) { fromWindow(w)->onFramebufferResize(width, height); });

    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);

    if (!audio_.init()) return false;

    visualizers_.add(std::make_unique<SpectrumBars>());
    visualizers_.add(std::make_unique<CircularSpectrum>());
    visualizers_.add(std::make_unique<FeedbackTunnel>());
    visualizers_.add(std::make_unique<Kaleidoscope>());
    visualizers_.add(std::make_unique<Plasma>());
    visualizers_.add(std::make_unique<Oscilloscope>());
    if (!visualizers_.init(fbWidth_, fbHeight_)) return false;

    if (!ui_.init(window_)) return false;

    if (const char* tourDir = std::getenv("MUSIC_APP_TOUR"); tourDir && *tourDir)
        tour_ = std::make_unique<DevTour>(tourDir);

    updateTitle();
    return true;
}

void App::shutdown() {
    audio_.shutdown();
    if (window_) {
        // GL objects die while the context is still alive.
        ui_.shutdown();
        visualizers_.clear();
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

void App::frame(float dt) {
    ui_.beginFrame();
    player_.update();

    analyzer_.update(audio_.samples(), audio_.sampleRate(), dt);
    visualizers_.update(analyzer_.features(), dt);
    visualizers_.render();

    ui_.draw(player_, visualizers_, dt);
    ui_.endFrame();

    titleTimer_ -= dt;
    if (titleTimer_ <= 0.0) updateTitle();
}

void App::updateTitle() {
    titleTimer_ = 0.25;
    std::string title = kAppName;
    if (const Track* track = player_.playlist().currentTrack(); track && audio_.hasTrack()) {
        title = track->displayName + "  [" + formatTime(audio_.position()) + " / " + formatTime(audio_.duration()) + "]";
        if (!audio_.isPlaying()) title += "  (paused)";
        title += "  - " + std::string(kAppName);
    }
    glfwSetWindowTitle(window_, title.c_str());
}

void App::toggleFullscreen() {
    if (!fullscreen_) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        GLFWmonitor* monitor = monitorForWindow(window_);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedWidth_, windowedHeight_, 0);
    }
    fullscreen_ = !fullscreen_;
    glfwSwapInterval(1);  // some drivers reset vsync on a mode change
}

void App::onKey(int key, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (ui_.wantsKeyboard()) return;  // typing in a text field
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    const bool pressed = action == GLFW_PRESS;
    const double seekStep = shift ? 30.0 : 5.0;

    switch (key) {
        case GLFW_KEY_ESCAPE:
            if (!pressed) break;
            if (fullscreen_) toggleFullscreen();  // first Esc leaves full screen, the next one quits
            else glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break;
        case GLFW_KEY_F:
        case GLFW_KEY_F11:    if (pressed) toggleFullscreen(); break;
        case GLFW_KEY_SPACE:  if (pressed) player_.togglePause(); break;
        case GLFW_KEY_RIGHT:  audio_.seek(audio_.position() + seekStep); break;
        case GLFW_KEY_LEFT:   audio_.seek(audio_.position() - seekStep); break;
        case GLFW_KEY_UP:     audio_.setVolume(audio_.volume() + 0.05f); break;
        case GLFW_KEY_DOWN:   audio_.setVolume(audio_.volume() - 0.05f); break;
        case GLFW_KEY_N:      if (pressed) { if (shift) player_.previous(); else player_.next(); } break;
        case GLFW_KEY_V:      if (pressed) { if (shift) visualizers_.previous(); else visualizers_.next(); } break;
        case GLFW_KEY_P:      if (pressed) ui_.togglePlaylist(); break;
        case GLFW_KEY_O:      if (pressed) ui_.openFileBrowser(); break;
        case GLFW_KEY_TAB:    if (pressed) ui_.toggleVisible(); break;
        default: return;
    }
    updateTitle();
}

void App::onDrop(int count, const char** paths) {
    player_.enqueue(std::vector<std::string>(paths, paths + count));
}

void App::onFramebufferResize(int width, int height) {
    fbWidth_ = width;
    fbHeight_ = height;
    if (width == 0 || height == 0) return;  // minimised
    visualizers_.resize(width, height);
}
