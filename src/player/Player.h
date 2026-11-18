#pragma once

#include "audio/AudioEngine.h"
#include "player/Playlist.h"

#include <string>
#include <vector>

// Playback logic on top of the audio engine: which track is playing, what
// comes next, and what the transport buttons do. Keyboard shortcuts, drag &
// drop and the on-screen controls all go through here so they behave the same.
class Player {
public:
    explicit Player(AudioEngine& audio) : audio_(audio) {}

    // Adds files and/or folders. If nothing was playing, starts the first new
    // track. Returns how many tracks were added.
    std::size_t enqueue(const std::vector<std::string>& paths);

    // Plays the track at `index`; if it can't be opened, tries the following
    // ones. Returns false if nothing could be played.
    bool play(std::size_t index);

    void togglePause();
    void next();
    // Restarts the current track if it's past the first few seconds, otherwise goes back one.
    void previous();

    // Call once per frame: moves on to the next track when one finishes.
    void update();

    AudioEngine& audio() { return audio_; }
    const AudioEngine& audio() const { return audio_; }
    Playlist& playlist() { return playlist_; }
    const Playlist& playlist() const { return playlist_; }

private:
    AudioEngine& audio_;
    Playlist playlist_;
    bool endHandled_ = false;
};
