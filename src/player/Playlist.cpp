#include "player/Playlist.h"

#include "player/Paths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

Track makeTrack(const fs::path& path) {
    return Track{pathToUtf8(path), pathToUtf8(path.stem())};
}

}  // namespace

bool Playlist::isSupported(const std::string& path) {
    std::string ext = pathToUtf8(utf8ToPath(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Formats miniaudio decodes out of the box.
    return ext == ".mp3" || ext == ".flac" || ext == ".wav";
}

std::size_t Playlist::add(const std::string& path) {
    const fs::path p = utf8ToPath(path);
    std::error_code ec;

    if (fs::is_directory(p, ec)) {
        std::vector<fs::path> found;
        const auto options = fs::directory_options::skip_permission_denied;
        for (fs::recursive_directory_iterator it(p, options, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && isSupported(pathToUtf8(it->path())))
                found.push_back(it->path());
        }
        std::sort(found.begin(), found.end());
        for (const auto& file : found) tracks_.push_back(makeTrack(file));
        return found.size();
    }

    if (fs::is_regular_file(p, ec) && isSupported(path)) {
        tracks_.push_back(makeTrack(p));
        return 1;
    }
    return 0;
}

void Playlist::remove(std::size_t index) {
    if (index >= tracks_.size()) return;
    tracks_.erase(tracks_.begin() + static_cast<std::ptrdiff_t>(index));

    if (!current_) return;
    if (*current_ == index) current_.reset();
    else if (*current_ > index) --*current_;
}

void Playlist::clear() {
    tracks_.clear();
    current_.reset();
}

std::optional<std::size_t> Playlist::nextIndex() const {
    if (tracks_.empty()) return std::nullopt;
    if (!current_) return 0;
    if (*current_ + 1 < tracks_.size()) return *current_ + 1;
    return repeat_ ? std::optional<std::size_t>(0) : std::nullopt;
}

std::optional<std::size_t> Playlist::previousIndex() const {
    if (tracks_.empty()) return std::nullopt;
    if (!current_) return 0;
    if (*current_ > 0) return *current_ - 1;
    return repeat_ ? std::optional<std::size_t>(tracks_.size() - 1) : std::nullopt;
}
