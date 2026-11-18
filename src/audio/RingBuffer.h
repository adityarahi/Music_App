#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// Single-producer / single-consumer ring buffer of stereo samples.
//
// The audio thread pushes every frame it plays. The render thread does not
// "consume" frames in order; it just takes a snapshot of the most recent N
// frames each video frame. That keeps the audio thread wait-free and means a
// slow render frame shows slightly older audio instead of stalling playback.
//
// The writer could in theory overwrite frames while the reader is copying, but
// with Capacity far larger than any snapshot (16384 vs <=4096 frames, ~250 ms
// of headroom at 48 kHz) that cannot happen in practice, and a torn read would
// only mean a glitch in one visual frame anyway.
template <std::size_t Capacity>
class StereoRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    // Audio thread only. `interleaved` holds frameCount L/R pairs.
    void write(const float* interleaved, std::size_t frameCount) {
        std::uint64_t w = writeIndex_.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < frameCount; ++i) {
            const std::size_t slot = static_cast<std::size_t>(w + i) & kMask;
            left_[slot]  = interleaved[2 * i];
            right_[slot] = interleaved[2 * i + 1];
        }
        writeIndex_.store(w + frameCount, std::memory_order_release);
    }

    // Render thread only. Copies the latest `count` frames (oldest first).
    // Frames that were never written read as silence.
    void readLatest(float* outLeft, float* outRight, std::size_t count) const {
        const std::uint64_t w = writeIndex_.load(std::memory_order_acquire);
        const std::size_t available = w < count ? static_cast<std::size_t>(w) : count;
        const std::size_t silent = count - available;

        for (std::size_t i = 0; i < silent; ++i)
            outLeft[i] = outRight[i] = 0.0f;

        const std::uint64_t start = w - available;
        for (std::size_t i = 0; i < available; ++i) {
            const std::size_t slot = static_cast<std::size_t>(start + i) & kMask;
            outLeft[silent + i]  = left_[slot];
            outRight[silent + i] = right_[slot];
        }
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    std::array<float, Capacity> left_{};
    std::array<float, Capacity> right_{};
    std::atomic<std::uint64_t> writeIndex_{0};
};
