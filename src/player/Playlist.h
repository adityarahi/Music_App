#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct Track {
    std::string path;          // UTF-8
    std::string displayName;   // file name without folder or extension
};

// Ordered list of tracks plus which one is current. Knows nothing about
// audio; the app asks it what to play next and tells the engine.
class Playlist {
public:
    // Adds a file, or every supported file inside a folder (recursively,
    // sorted by path). Returns how many tracks were added.
    std::size_t add(const std::string& path);

    void remove(std::size_t index);
    void clear();

    const std::vector<Track>& tracks() const { return tracks_; }
    bool empty() const { return tracks_.empty(); }
    std::size_t size() const { return tracks_.size(); }

    std::optional<std::size_t> current() const { return current_; }
    void setCurrent(std::optional<std::size_t> index) { current_ = index; }
    const Track* currentTrack() const { return current_ ? &tracks_[*current_] : nullptr; }

    // Index of the track after / before the current one, honouring repeat.
    std::optional<std::size_t> nextIndex() const;
    std::optional<std::size_t> previousIndex() const;

    bool repeat() const { return repeat_; }
    void setRepeat(bool on) { repeat_ = on; }

    static bool isSupported(const std::string& path);

private:
    std::vector<Track> tracks_;
    std::optional<std::size_t> current_;
    bool repeat_ = true;
};
