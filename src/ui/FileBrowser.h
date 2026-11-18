#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

// A small file picker drawn with ImGui, so adding music works the same on
// every platform without native dialog libraries. Shows folders and
// supported audio files; double-click a folder to open it, a file to add it.
class FileBrowser {
public:
    void open();
    bool isOpen() const { return open_; }

    // Draws the window if open. Returns paths (UTF-8) the user chose to add this frame.
    std::vector<std::string> draw();

private:
    struct Entry {
        std::string name;   // UTF-8, for display
        std::filesystem::path path;
        bool isDirectory;
    };

    void navigate(const std::filesystem::path& directory);
    void refresh();

    bool open_ = false;
    std::filesystem::path directory_;
    std::vector<Entry> entries_;
    std::set<std::size_t> selected_;
    std::string error_;
    char pathInput_[1024] = {};
};
