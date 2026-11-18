#include "ui/FileBrowser.h"

#include "player/Paths.h"
#include "player/Playlist.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path homeDirectory() {
#ifdef _WIN32
    if (const wchar_t* profile = _wgetenv(L"USERPROFILE")) return fs::path(profile);
#else
    if (const char* home = std::getenv("HOME")) return fs::path(home);
#endif
    std::error_code ec;
    return fs::current_path(ec);
}

}  // namespace

void FileBrowser::open() {
    open_ = true;
    if (directory_.empty()) navigate(homeDirectory());
    else refresh();
}

void FileBrowser::navigate(const fs::path& directory) {
    std::error_code ec;
    fs::path target = fs::absolute(directory, ec);
    if (ec || !fs::is_directory(target, ec)) {
        error_ = "Not a folder: " + pathToUtf8(directory);
        return;
    }
    directory_ = target.lexically_normal();
    refresh();
}

void FileBrowser::refresh() {
    entries_.clear();
    selected_.clear();
    error_.clear();

    const std::string dirText = pathToUtf8(directory_);
    std::snprintf(pathInput_, sizeof(pathInput_), "%s", dirText.c_str());

    std::error_code ec;
    for (fs::directory_iterator it(directory_, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        std::error_code entryEc;
        const bool isDir = it->is_directory(entryEc);
        std::string name = pathToUtf8(it->path().filename());
        if (name.empty() || name[0] == '.') continue;  // hidden files
        if (isDir || (it->is_regular_file(entryEc) && Playlist::isSupported(name)))
            entries_.push_back({std::move(name), it->path(), isDir});
    }
    if (ec) error_ = "Cannot read this folder: " + ec.message();

    // Folders first, then files, each alphabetically (case-insensitive).
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(),
                                            [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
    });
}

std::vector<std::string> FileBrowser::draw() {
    std::vector<std::string> chosen;
    if (!open_) return chosen;

    const float em = ImGui::GetFontSize();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 38.0f, em * 28.0f), ImGuiCond_Appearing);

    if (!ImGui::Begin("Add music", &open_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return chosen;
    }

    // Navigation row: Up, Home, (Windows drives), editable path.
    if (ImGui::Button("Up") && directory_.has_parent_path() && directory_.parent_path() != directory_)
        navigate(directory_.parent_path());
    ImGui::SameLine();
    if (ImGui::Button("Home")) navigate(homeDirectory());
#ifdef _WIN32
    ImGui::SameLine();
    ImGui::SetNextItemWidth(em * 4.0f);
    const std::string currentDrive = pathToUtf8(directory_.root_name());
    if (ImGui::BeginCombo("##drive", currentDrive.c_str())) {
        const DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (!(drives & (1u << i))) continue;
            const std::string drive = std::string(1, static_cast<char>('A' + i)) + ":";
            if (ImGui::Selectable(drive.c_str(), drive == currentDrive)) navigate(utf8ToPath(drive + "\\"));
        }
        ImGui::EndCombo();
    }
#endif
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##path", pathInput_, sizeof(pathInput_), ImGuiInputTextFlags_EnterReturnsTrue))
        navigate(utf8ToPath(pathInput_));

    if (!error_.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", error_.c_str());

    // File list.
    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("entries", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
        std::optional<fs::path> enter;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const Entry& entry = entries_[i];
            const bool isSelected = selected_.count(i) > 0;
            const std::string label = (entry.isDirectory ? entry.name + "/" : entry.name) + "##" + std::to_string(i);

            if (entry.isDirectory) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
            if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (entry.isDirectory) enter = entry.path;
                    else chosen.push_back(pathToUtf8(entry.path));
                } else if (ImGui::GetIO().KeyCtrl) {
                    if (isSelected) selected_.erase(i);
                    else selected_.insert(i);
                } else {
                    selected_ = {i};
                }
            }
            if (entry.isDirectory) ImGui::PopStyleColor();
        }
        if (entries_.empty() && error_.empty()) ImGui::TextDisabled("No folders or audio files here.");
        if (enter) navigate(*enter);  // after the loop: navigating rebuilds entries_
    }
    ImGui::EndChild();

    // Footer buttons.
    ImGui::BeginDisabled(selected_.empty());
    const std::string addLabel = "Add selected (" + std::to_string(selected_.size()) + ")";
    if (ImGui::Button(addLabel.c_str())) {
        for (std::size_t i : selected_) chosen.push_back(pathToUtf8(entries_[i].path));
        selected_.clear();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Add whole folder")) chosen.push_back(pathToUtf8(directory_));
    ImGui::SameLine();
    ImGui::TextDisabled("Double-click a song to add it, Ctrl+click to pick several.");

    ImGui::End();
    return chosen;
}
