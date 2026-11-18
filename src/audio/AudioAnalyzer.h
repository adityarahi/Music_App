#pragma once

#include "audio/AudioEngine.h"

#include <array>
#include <cstddef>

// Everything a visualizer gets to know about the music, refreshed once per
// rendered frame. Spectrum, bands and beat detection get added here when the
// FFT lands; visualizers only ever see this struct, never the audio engine.
struct AudioFeatures {
    static constexpr std::size_t kWaveformSize = 1024;

    std::array<float, kWaveformSize> waveformLeft{};   // -1..1, oldest first
    std::array<float, kWaveformSize> waveformRight{};

    float rms = 0.0f;          // instantaneous loudness of this window, 0..~1
    float level = 0.0f;        // rms with fast attack / slow release, for smooth motion
    float time = 0.0f;         // seconds since the app started
};

class AudioAnalyzer {
public:
    // Call once per frame on the render thread.
    void update(const AudioEngine::SampleBuffer& samples, float dt);

    const AudioFeatures& features() const { return features_; }

private:
    AudioFeatures features_;
};
