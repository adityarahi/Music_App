#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

// The little markers above spectrum bars: they snap up to a bar's height,
// hang there briefly, then fall with gravity until they land on the bar.
template <std::size_t N>
class PeakCaps {
public:
    void update(const std::array<float, N>& values, float dt) {
        for (std::size_t i = 0; i < N; ++i) {
            if (values[i] >= peaks_[i]) {
                peaks_[i] = values[i];
                velocity_[i] = 0.0f;
                hold_[i] = kHoldSeconds;
            } else if (hold_[i] > 0.0f) {
                hold_[i] -= dt;
            } else {
                velocity_[i] += kGravity * dt;
                peaks_[i] = std::max(values[i], peaks_[i] - velocity_[i] * dt);
            }
        }
    }

    float operator[](std::size_t i) const { return peaks_[i]; }

private:
    static constexpr float kHoldSeconds = 0.35f;
    static constexpr float kGravity = 2.2f;   // value units per second squared

    std::array<float, N> peaks_{};
    std::array<float, N> velocity_{};
    std::array<float, N> hold_{};
};
