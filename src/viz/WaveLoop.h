#pragma once

#include <array>
#include <cstddef>

// Turns a waveform window into N values suitable for drawing as a closed loop
// (e.g. a ring whose radius wobbles with the sound):
//  - each point averages a few samples, so the loop isn't jagged,
//  - the end is blended toward the start, so there's no visible seam,
//  - `smoothPasses` rounds of [1 2 1] smoothing remove the remaining spikes
//    (more passes = rounder, calmer shape).
template <std::size_t N>
std::array<float, N> waveLoop(const float* samples, std::size_t sampleCount, int smoothPasses = 1) {
    const std::size_t perPoint = sampleCount / N;

    std::array<float, N> raw{};
    for (std::size_t i = 0; i < N; ++i) {
        float sum = 0.0f;
        for (std::size_t j = 0; j < perPoint; ++j) sum += samples[i * perPoint + j];
        raw[i] = sum / static_cast<float>(perPoint);
    }

    constexpr std::size_t kSeam = N / 10;
    for (std::size_t i = 0; i < kSeam; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(kSeam + 1);
        float& v = raw[N - kSeam + i];
        v = v * (1.0f - t) + raw[0] * t;
    }

    std::array<float, N> smooth{};
    for (int pass = 0; pass < smoothPasses; ++pass) {
        for (std::size_t i = 0; i < N; ++i)
            smooth[i] = 0.25f * raw[(i + N - 1) % N] + 0.5f * raw[i] + 0.25f * raw[(i + 1) % N];
        raw = smooth;
    }
    return raw;
}
