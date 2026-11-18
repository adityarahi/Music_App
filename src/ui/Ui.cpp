#include "ui/Ui.h"

#include "platform/Platform.h"
#include "player/Player.h"
#include "viz/VisualizerManager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

namespace {

constexpr ImVec4 kAccent{0.47f, 0.55f, 1.0f, 1.0f};

enum class Icon { Previous, Play, Pause, Next, Fullscreen };

std::string formatTime(double seconds) {
    const int total = static_cast<int>(std::max(seconds, 0.0));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

// Shortens `text` with "..." so it fits in `maxWidth` pixels.
std::string ellipsize(const std::string& text, float maxWidth) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;
    std::string out = text;
    while (!out.empty() && ImGui::CalcTextSize((out + "...").c_str()).x > maxWidth) {
        out.pop_back();
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();  // keep UTF-8 intact
    }
    return out + "...";
}

// A round button with a drawn transport icon (the default font has no such glyphs).
bool iconButton(const char* id, Icon icon, float size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 c(pos.x + size * 0.5f, pos.y + size * 0.5f);
    const bool primary = icon == Icon::Play || icon == Icon::Pause;

    ImVec4 bg = primary ? kAccent : ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    if (hovered) bg.w = primary ? 0.85f : 0.18f;
    if (held) bg.w = primary ? 0.7f : 0.28f;
    draw->AddCircleFilled(c, size * 0.5f, ImGui::GetColorU32(bg));

    const ImU32 fg = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.95f));
    const float s = size * (primary ? 0.22f : 0.18f);

    switch (icon) {
        case Icon::Play:
            draw->AddTriangleFilled(ImVec2(c.x - s * 0.8f, c.y - s), ImVec2(c.x - s * 0.8f, c.y + s), ImVec2(c.x + s * 1.1f, c.y), fg);
            break;
        case Icon::Pause:
            draw->AddRectFilled(ImVec2(c.x - s * 0.9f, c.y - s), ImVec2(c.x - s * 0.25f, c.y + s), fg, s * 0.15f);
            draw->AddRectFilled(ImVec2(c.x + s * 0.25f, c.y - s), ImVec2(c.x + s * 0.9f, c.y + s), fg, s * 0.15f);
            break;
        case Icon::Previous:
            draw->AddRectFilled(ImVec2(c.x - s * 1.1f, c.y - s), ImVec2(c.x - s * 0.75f, c.y + s), fg);
            draw->AddTriangleFilled(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s, c.y + s), ImVec2(c.x - s * 0.75f, c.y), fg);
            break;
        case Icon::Next:
            draw->AddRectFilled(ImVec2(c.x + s * 0.75f, c.y - s), ImVec2(c.x + s * 1.1f, c.y + s), fg);
            draw->AddTriangleFilled(ImVec2(c.x - s, c.y - s), ImVec2(c.x - s, c.y + s), ImVec2(c.x + s * 0.75f, c.y), fg);
            break;
        case Icon::Fullscreen: {
            // Four corner brackets.
            const float e = s * 1.2f, l = s * 0.6f, t = std::max(1.5f, size * 0.07f);
            for (const float sx : {-1.0f, 1.0f}) {
                for (const float sy : {-1.0f, 1.0f}) {
                    const ImVec2 corner(c.x + sx * e, c.y + sy * e);
                    draw->AddLine(corner, ImVec2(corner.x - sx * l, corner.y), fg, t);
                    draw->AddLine(corner, ImVec2(corner.x, corner.y - sy * l), fg, t);
                }
            }
            break;
        }
    }
    return pressed;
}

void applyStyle(float scale) {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(14.0f, 10.0f);
    style.GrabMinSize = 12.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.09f, 0.82f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.13f, 0.96f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.09f, 0.16f, 0.95f);
    c[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.12f);
    c[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.65f, 0.72f, 1.0f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    c[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
    c[ImGuiCol_ButtonActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.24f);
    c[ImGuiCol_Header] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f);
    c[ImGuiCol_HeaderHovered] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.22f);
    c[ImGuiCol_HeaderActive] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f);
    c[ImGuiCol_CheckMark] = kAccent;

    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.FontSizeBase = 17.0f;
}

}  // namespace

bool Ui::init(GLFWwindow* window) {
    window_ = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // don't leave imgui.ini files next to the music
    io.Fonts->AddFontDefaultVector();

    applyStyle(ImGui_ImplGlfw_GetContentScaleForWindow(window));

    // Our own GLFW callbacks are already installed; ImGui chains to them.
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true) || !ImGui_ImplOpenGL3_Init("#version 330")) {
        std::fprintf(stderr, "Failed to initialise the UI.\n");
        return false;
    }
    initialised_ = true;
    return true;
}

void Ui::shutdown() {
    if (!initialised_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialised_ = false;
}

void Ui::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Ui::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Ui::wantsKeyboard() const {
    // Only while typing; clicking a button shouldn't disable Space and the arrows.
    return initialised_ && ImGui::GetIO().WantTextInput;
}

void Ui::updateVisibility(float dt) {
    const ImGuiIO& io = ImGui::GetIO();
    bool activity = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || io.MouseWheel != 0.0f;
    for (const bool down : io.MouseDown) activity = activity || down;
    idleSeconds_ = activity ? 0.0f : idleSeconds_ + dt;

    // Stay up while the user is clearly using the UI.
    const bool pinned = playlistOpen_ || settingsOpen_ || browser_.isOpen() || seeking_ || io.WantCaptureMouse;
    const bool show = visible_ && (idleSeconds_ < kHideAfterSeconds || pinned);

    // Appear quickly, fade out gently.
    const float speed = show ? 1.0f / 0.15f : 1.0f / 0.6f;
    alpha_ = std::clamp(alpha_ + (show ? speed : -speed) * dt, 0.0f, 1.0f);

    const bool hideCursor = idleSeconds_ >= kHideAfterSeconds && !pinned;
    if (hideCursor != cursorHidden_) {
        cursorHidden_ = hideCursor;
        glfwSetInputMode(window_, GLFW_CURSOR, hideCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
    }
}

void Ui::draw(Player& player, VisualizerManager& visualizers, float dt) {
    updateVisibility(dt);

    // Over the visualization (not the controls): wheel changes the volume,
    // double-click toggles full screen.
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseWheel != 0.0f)
        player.audio().setVolume(player.audio().volume() + 0.05f * io.MouseWheel);
    if (!io.WantCaptureMouse && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        fullscreenRequested_ = true;

    if (alpha_ <= 0.01f) return;
    ImGui::GetStyle().Alpha = alpha_;

    drawControlBar(player, visualizers);
    if (playlistOpen_) drawPlaylist(player);
    if (settingsOpen_) drawSettings(visualizers);

    const auto chosen = browser_.draw();
    if (!chosen.empty()) player.enqueue(chosen);
}

void Ui::drawControlBar(Player& player, VisualizerManager& visualizers) {
    AudioEngine& audio = player.audio();
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float em = ImGui::GetFontSize();
    const float iconSize = em * 2.0f;
    const float margin = em * 0.6f;

    barHeight_ = style.WindowPadding.y * 2.0f + ImGui::GetFrameHeight() + style.ItemSpacing.y + iconSize;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + margin, viewport->WorkPos.y + viewport->WorkSize.y - margin),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - 2.0f * margin, barHeight_));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ImGui::Begin("##controls", nullptr, flags)) {
        ImGui::End();
        return;
    }

    // Row 1: position, seek bar, duration. Seeking happens on release so a drag
    // doesn't restart decoding dozens of times.
    const double duration = audio.duration();
    if (!seeking_) seekSeconds_ = static_cast<float>(audio.position());
    const std::string elapsed = formatTime(seekSeconds_);
    const std::string total = formatTime(duration);
    const float timeWidth = ImGui::CalcTextSize("00:00").x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(elapsed.c_str());
    ImGui::SameLine(timeWidth + style.WindowPadding.x + style.ItemSpacing.x);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - timeWidth - style.ItemSpacing.x);
    ImGui::BeginDisabled(!audio.hasTrack());
    ImGui::SliderFloat("##seek", &seekSeconds_, 0.0f, std::max(static_cast<float>(duration), 0.01f), "");
    seeking_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) audio.seek(seekSeconds_);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextUnformatted(total.c_str());

    // Row 2, left: transport buttons and the track name.
    const float rowY = ImGui::GetCursorPosY();
    const float rowEndX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    if (iconButton("##prev", Icon::Previous, iconSize)) player.previous();
    ImGui::SameLine();
    if (iconButton("##play", audio.isPlaying() ? Icon::Pause : Icon::Play, iconSize)) player.togglePause();
    ImGui::SameLine();
    if (iconButton("##next", Icon::Next, iconSize)) player.next();

    // Row 2, right: playlist toggle, visualizer switcher, volume.
    const float buttonH = ImGui::GetFrameHeight();
    const float vizNameWidth = em * 9.0f;
    const float volumeWidth = em * 7.0f;
    const float playlistWidth = ImGui::CalcTextSize("Playlist").x + style.FramePadding.x * 2.0f;
    const float settingsWidth = ImGui::CalcTextSize("Settings").x + style.FramePadding.x * 2.0f;
    const float rightWidth = playlistWidth + settingsWidth + buttonH * 3.0f + vizNameWidth + volumeWidth +
                             ImGui::CalcTextSize("Vol").x + style.ItemSpacing.x * 8.0f;
    const float rightX = rowEndX - rightWidth;
    const float centeredY = rowY + (iconSize - buttonH) * 0.5f;

    // Track name between the two groups, vertically centred on the icons.
    ImGui::SameLine();
    const float titleX = ImGui::GetCursorPosX() + em * 0.5f;
    const Track* track = player.playlist().currentTrack();
    const std::string title = track ? track->displayName : "No music yet: drop files or folders here, or open the Playlist";
    ImGui::SetCursorPos(ImVec2(titleX, rowY + (iconSize - ImGui::GetTextLineHeight()) * 0.5f));
    if (track) ImGui::TextUnformatted(ellipsize(title, rightX - titleX - em).c_str());
    else ImGui::TextDisabled("%s", ellipsize(title, rightX - titleX - em).c_str());

    ImGui::SetCursorPos(ImVec2(rightX, centeredY));
    if (ImGui::Button("Playlist")) togglePlaylist();
    ImGui::SameLine();
    if (ImGui::Button("Settings")) toggleSettings();
    ImGui::SameLine();
    if (ImGui::ArrowButton("##vizprev", ImGuiDir_Left)) visualizers.previous();
    ImGui::SameLine();
    {
        const char* name = visualizers.current().name();
        const float textWidth = ImGui::CalcTextSize(name).x;
        const ImVec2 start = ImGui::GetCursorPos();
        ImGui::Dummy(ImVec2(vizNameWidth, buttonH));
        ImGui::SetCursorPos(ImVec2(start.x + (vizNameWidth - textWidth) * 0.5f, start.y + style.FramePadding.y));
        ImGui::TextUnformatted(name);
        ImGui::SetCursorPos(ImVec2(start.x + vizNameWidth, start.y));
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##viznext", ImGuiDir_Right)) visualizers.next();
    ImGui::SameLine();
    ImGui::TextUnformatted("Vol");  // same line as framed widgets, so already baseline-aligned
    ImGui::SameLine();
    ImGui::SetNextItemWidth(volumeWidth);
    float volume = audio.volume() * 100.0f;
    if (ImGui::SliderFloat("##volume", &volume, 0.0f, 100.0f, "%.0f%%")) audio.setVolume(volume / 100.0f);
    ImGui::SameLine();
    if (iconButton("##fullscreen", Icon::Fullscreen, buttonH)) fullscreenRequested_ = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Full screen (F)");

    ImGui::End();
}

void Ui::drawPlaylist(Player& player) {
    Playlist& playlist = player.playlist();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float em = ImGui::GetFontSize();
    const float margin = em * 0.6f;

    const float width = std::min(std::max(em * 22.0f, viewport->WorkSize.x * 0.3f), viewport->WorkSize.x - 2.0f * margin);
    const float height = viewport->WorkSize.y - barHeight_ - 3.0f * margin;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin - width, viewport->WorkPos.y + margin));
    ImGui::SetNextWindowSize(ImVec2(width, std::max(height, em * 6.0f)));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin("Playlist", &playlistOpen_, flags)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add files...")) browser_.open();
    ImGui::SameLine();
    ImGui::BeginDisabled(playlist.empty());
    if (ImGui::Button("Clear")) {
        playlist.clear();
        player.audio().unload();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool repeat = playlist.repeat();
    if (ImGui::Checkbox("Repeat", &repeat)) playlist.setRepeat(repeat);
    ImGui::Separator();

    if (playlist.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("Drag MP3 files or folders onto the window,");
        ImGui::TextDisabled("or click \"Add files...\".");
        ImGui::End();
        return;
    }

    std::optional<std::size_t> toPlay;
    std::optional<std::size_t> toRemove;
    const auto current = playlist.current();

    if (ImGui::BeginTable("tracks", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000").x);
        ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);

        const auto& tracks = playlist.tracks();
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool isCurrent = current && *current == i;
            const std::string label = std::to_string(i + 1) + "##row" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), isCurrent,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick) &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                toPlay = i;
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Play")) toPlay = i;
                if (ImGui::MenuItem("Remove")) toRemove = i;
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            if (isCurrent) ImGui::TextColored(kAccent, "%s", tracks[i].displayName.c_str());
            else ImGui::TextUnformatted(tracks[i].displayName.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();

    // Apply after drawing so the list isn't modified while it's being iterated.
    if (toRemove) {
        if (current && *current == *toRemove) player.audio().unload();
        playlist.remove(*toRemove);
    }
    if (toPlay) player.play(*toPlay);
}

void Ui::drawSettings(VisualizerManager& visualizers) {
    const float em = ImGui::GetFontSize();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = em * 0.6f;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + margin, viewport->WorkPos.y + margin), ImGuiCond_Appearing);
    // Grows to fit its contents but never past the control bar; scrolls beyond that.
    const float maxHeight = std::max(viewport->WorkSize.y - barHeight_ - 3.0f * margin, em * 8.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(em * 20.0f, 0.0f), ImVec2(em * 30.0f, maxHeight));

    if (!ImGui::Begin("Settings", &settingsOpen_, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Visualization");
    for (std::size_t i = 0; i < visualizers.count(); ++i) {
        if (ImGui::RadioButton(visualizers.at(i).name(), visualizers.currentIndex() == i)) visualizers.select(i);
    }

    if (platform::supportsOpenWith()) {
        ImGui::SeparatorText("Windows integration");
        const bool registered = platform::isOpenWithRegistered();
        ImGui::TextWrapped(registered ? "Dhundarsh is in the \"Open with\" menu for MP3, FLAC and WAV files."
                                      : "Add Dhundarsh to the \"Open with\" menu for MP3, FLAC and WAV files.");

        // Registering a copy that lives inside WSL only works while WSL is running.
        const std::string exe = platform::executablePath();
        if (exe.rfind("\\\\", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.35f, 1.0f));
            ImGui::TextWrapped("This copy is running from inside WSL. Copy dhundarsh.exe to a Windows folder "
                               "(for example C:\\Apps\\Dhundarsh) and register from there.");
            ImGui::PopStyleColor();
        }

        if (!registered) {
            if (ImGui::Button("Add to \"Open with\"")) {
                std::string error;
                openWithMessage_ = platform::registerOpenWith(error)
                                       ? "Done. Right-click a song > Open with > Dhundarsh."
                                       : error;
            }
        } else if (ImGui::Button("Remove from \"Open with\"")) {
            platform::unregisterOpenWith();
            openWithMessage_ = "Removed.";
        }
        if (!openWithMessage_.empty()) ImGui::TextDisabled("%s", openWithMessage_.c_str());
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Keyboard shortcuts") && ImGui::BeginTable("keys", 2, ImGuiTableFlags_SizingFixedFit)) {
        static constexpr const char* kKeys[][2] = {
            {"Space", "Play / pause"},
            {"Left / Right", "Seek 5 s (Shift: 30 s)"},
            {"Up / Down, wheel", "Volume"},
            {"N / Shift+N", "Next / previous track"},
            {"V / Shift+V", "Next / previous visualizer"},
            {"F, F11, double-click", "Full screen"},
            {"P", "Playlist"},
            {"S", "Settings"},
            {"O", "Add files"},
            {"Tab", "Hide / show controls"},
            {"Esc", "Leave full screen, or quit"},
        };
        for (const auto& row : kKeys) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(kAccent, "%s", row[0]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(row[1]);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}
