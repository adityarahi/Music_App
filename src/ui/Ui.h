#pragma once

#include "ui/FileBrowser.h"

struct GLFWwindow;
class Player;
class VisualizerManager;

// On-screen controls drawn with Dear ImGui over the visualizer: a transport
// bar along the bottom, a playlist panel, and the file browser.
//
// The controls fade out (and the mouse cursor hides) after a few seconds
// without mouse movement, like a video player, and come back on any movement.
class Ui {
public:
    bool init(GLFWwindow* window);
    void shutdown();

    // Per frame, in this order: beginFrame() early, draw() after the
    // visualizers have rendered, then endFrame() to put the UI on screen.
    void beginFrame();
    void draw(Player& player, VisualizerManager& visualizers, float dt);
    void endFrame();

    // True while a text field has focus, so global shortcuts should be ignored.
    bool wantsKeyboard() const;

    void toggleVisible() { visible_ = !visible_; }
    void togglePlaylist() { playlistOpen_ = !playlistOpen_; }
    void openFileBrowser() { browser_.open(); }

    // Used by the screenshot tour.
    void setPlaylistOpen(bool open) { playlistOpen_ = open; }
    void hideUntilMouseMoves() { idleSeconds_ = kHideAfterSeconds; }

private:
    static constexpr float kHideAfterSeconds = 3.0f;

    void updateVisibility(float dt);
    void drawControlBar(Player& player, VisualizerManager& visualizers);
    void drawPlaylist(Player& player);

    GLFWwindow* window_ = nullptr;
    bool initialised_ = false;

    FileBrowser browser_;
    bool visible_ = true;        // Tab toggles
    bool playlistOpen_ = false;
    float idleSeconds_ = 0.0f;
    float alpha_ = 1.0f;         // current fade level of all UI
    bool cursorHidden_ = false;

    bool seeking_ = false;       // seek slider being dragged
    float seekSeconds_ = 0.0f;
    float barHeight_ = 0.0f;     // last control bar height, so the playlist sits above it
};
