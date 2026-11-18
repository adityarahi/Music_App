#include "player/Player.h"

#include <cstdio>

std::size_t Player::enqueue(const std::vector<std::string>& paths) {
    const std::size_t firstNew = playlist_.size();
    std::size_t added = 0;
    for (const auto& path : paths) {
        const std::size_t n = playlist_.add(path);
        if (n == 0) std::fprintf(stderr, "Skipped (not a supported audio file or folder): %s\n", path.c_str());
        added += n;
    }

    if (added > 0) std::printf("Added %zu track(s) to the playlist\n", added);
    const bool idle = !audio_.hasTrack() || audio_.isAtEnd();  // a paused track keeps its place
    if (added > 0 && idle) play(firstNew);
    return added;
}

bool Player::play(std::size_t index) {
    const std::size_t count = playlist_.size();
    for (std::size_t attempt = 0; attempt < count; ++attempt) {
        const std::size_t i = (index + attempt) % count;
        if (audio_.load(playlist_.tracks()[i].path)) {
            playlist_.setCurrent(i);
            endHandled_ = false;
            std::printf("Playing: %s\n", playlist_.tracks()[i].displayName.c_str());
            return true;
        }
    }
    return false;
}

void Player::togglePause() {
    if (audio_.hasTrack()) {
        audio_.togglePause();
    } else if (!playlist_.empty()) {
        play(playlist_.current().value_or(0));
    }
}

void Player::next() {
    if (const auto index = playlist_.nextIndex()) play(*index);
}

void Player::previous() {
    if (audio_.position() > 3.0 || !playlist_.previousIndex()) {
        audio_.seek(0.0);
        return;
    }
    play(*playlist_.previousIndex());
}

void Player::update() {
    if (!audio_.isAtEnd()) {
        endHandled_ = false;
        return;
    }
    if (endHandled_) return;
    endHandled_ = true;  // at the end of the list with repeat off, stay stopped

    if (const auto index = playlist_.nextIndex()) play(*index);
}
