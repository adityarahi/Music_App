#pragma once

#include "audio/AudioEngine.h"

#include <array>
#include <cstddef>

struct kiss_fftr_state;

// Everything a visualizer gets to know about the music, refreshed once per
// rendered frame. Visualizers only ever see this struct, never the audio engine.
// All 0..1 values are already smoothed for display: they jump up instantly
// and fall back gradually.
struct AudioFeatures {
    static constexpr std::size_t kWaveformSize = 1024;
    static constexpr std::size_t kFftSize = 2048;
    static constexpr std::size_t kSpectrumSize = kFftSize / 2;
    static constexpr std::size_t kBandCount = 32;

    std::array<float, kWaveformSize> waveformLeft{};   // -1..1, oldest first
    std::array<float, kWaveformSize> waveformRight{};

    // 0..1 per FFT bin; bin i is centred on i * sampleRate / kFftSize Hz.
    std::array<float, kSpectrumSize> spectrum{};

    // 0..1, log-spaced from ~40 Hz to ~16 kHz, so each band covers a similar
    // musical range (bass gets as many bands as treble).
    std::array<float, kBandCount> bands{};

    float bass = 0.0f;         // 0..1, below ~250 Hz
    float mid = 0.0f;          // 0..1, ~250 Hz - 4 kHz
    float treble = 0.0f;       // 0..1, above ~4 kHz

    float rms = 0.0f;          // instantaneous loudness of the waveform window, 0..~1
    float level = 0.0f;        // smoothed rms

    bool beat = false;         // true only on the frame a kick/bass hit is detected
    float beatPulse = 0.0f;    // 1 on a beat, decays to 0 over ~0.3 s

    float time = 0.0f;         // seconds since the app started
};

class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    AudioAnalyzer(const AudioAnalyzer&) = delete;
    AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;

    // Call once per frame on the render thread.
    void update(const AudioEngine::SampleBuffer& samples, unsigned sampleRate, float dt);

    const AudioFeatures& features() const { return features_; }

private:
    void computeLoudness(float dt);
    void computeSpectrum(float dt);
    void computeBands(unsigned sampleRate, float dt);
    void detectBeat(unsigned sampleRate, float dt);

    AudioFeatures features_;

    kiss_fftr_state* fft_ = nullptr;
    std::array<float, AudioFeatures::kFftSize> left_{};
    std::array<float, AudioFeatures::kFftSize> right_{};
    std::array<float, AudioFeatures::kFftSize> window_{};     // Hann window
    std::array<float, AudioFeatures::kFftSize> fftInput_{};
    std::array<float, AudioFeatures::kSpectrumSize + 1> magnitude_{};  // linear amplitude per bin
    float windowGain_ = 1.0f;

    // FFT bin range [first, last] of each band, recomputed if the sample rate changes.
    std::array<std::size_t, AudioFeatures::kBandCount> bandFirstBin_{};
    std::array<std::size_t, AudioFeatures::kBandCount> bandLastBin_{};
    std::array<float, AudioFeatures::kBandCount> bandCenterHz_{};
    unsigned bandSampleRate_ = 0;

    float bassEnergyAverage_ = 0.0f;
    float sinceLastBeat_ = 0.0f;
};
