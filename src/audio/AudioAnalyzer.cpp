#include "audio/AudioAnalyzer.h"

#include <kiss_fftr.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using F = AudioFeatures;

constexpr float kMinDb = -72.0f;   // maps to 0 on screen
constexpr float kMaxDb = -12.0f;   // maps to 1 on screen
constexpr float kTiltDbPerOctave = 3.0f;  // music gets quieter with pitch; lift the highs to even out the bars

constexpr float kLowestBandHz = 40.0f;
constexpr float kHighestBandHz = 16000.0f;

// Exponential fall-off that is the same at any frame rate.
float decayFactor(float dt, float halfLife) { return std::exp2(-dt / halfLife); }

void riseFastFallSlow(float& value, float target, float decay) { value = std::max(target, value * decay); }

float toUnit(float amplitude, float freqHz) {
    float db = 20.0f * std::log10(amplitude + 1e-9f);
    db += kTiltDbPerOctave * std::log2(std::max(freqHz, 20.0f) / 1000.0f);
    return std::clamp((db - kMinDb) / (kMaxDb - kMinDb), 0.0f, 1.0f);
}

}  // namespace

AudioAnalyzer::AudioAnalyzer() {
    fft_ = kiss_fftr_alloc(static_cast<int>(F::kFftSize), 0, nullptr, nullptr);

    // Hann window: tapers each block to zero at the edges so the FFT doesn't
    // see a hard cut, which would smear energy across all bins (flickery bars).
    float sum = 0.0f;
    for (std::size_t i = 0; i < F::kFftSize; ++i) {
        const float phase = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(F::kFftSize - 1);
        window_[i] = 0.5f - 0.5f * std::cos(phase);
        sum += window_[i];
    }
    // Scales bin magnitudes so a full-scale sine reads as amplitude ~1.
    windowGain_ = 2.0f / sum;
}

AudioAnalyzer::~AudioAnalyzer() { kiss_fftr_free(fft_); }

void AudioAnalyzer::update(const AudioEngine::SampleBuffer& samples, unsigned sampleRate, float dt) {
    features_.time += dt;
    if (sampleRate == 0) return;

    samples.readLatest(left_.data(), right_.data(), F::kFftSize);

    // The waveform is the most recent part of the FFT block.
    constexpr std::size_t offset = F::kFftSize - F::kWaveformSize;
    std::copy_n(left_.begin() + offset, F::kWaveformSize, features_.waveformLeft.begin());
    std::copy_n(right_.begin() + offset, F::kWaveformSize, features_.waveformRight.begin());

    computeLoudness(dt);
    computeSpectrum(dt);
    computeBands(sampleRate, dt);
    detectBeat(sampleRate, dt);
}

void AudioAnalyzer::computeLoudness(float dt) {
    double sumSquares = 0.0;
    for (std::size_t i = 0; i < F::kWaveformSize; ++i) {
        const float mono = 0.5f * (features_.waveformLeft[i] + features_.waveformRight[i]);
        sumSquares += static_cast<double>(mono) * mono;
    }
    features_.rms = static_cast<float>(std::sqrt(sumSquares / F::kWaveformSize));
    riseFastFallSlow(features_.level, features_.rms, decayFactor(dt, 0.15f));
}

void AudioAnalyzer::computeSpectrum(float dt) {
    for (std::size_t i = 0; i < F::kFftSize; ++i)
        fftInput_[i] = 0.5f * (left_[i] + right_[i]) * window_[i];

    std::array<kiss_fft_cpx, F::kSpectrumSize + 1> bins;
    kiss_fftr(fft_, fftInput_.data(), bins.data());

    for (std::size_t k = 0; k <= F::kSpectrumSize; ++k)
        magnitude_[k] = std::sqrt(bins[k].r * bins[k].r + bins[k].i * bins[k].i) * windowGain_;

    // The display spectrum ignores the per-octave tilt so it stays a plain
    // magnitude view; bands below apply it.
    const float decay = decayFactor(dt, 0.1f);
    for (std::size_t k = 0; k < F::kSpectrumSize; ++k)
        riseFastFallSlow(features_.spectrum[k], toUnit(magnitude_[k], 1000.0f), decay);
}

void AudioAnalyzer::computeBands(unsigned sampleRate, float dt) {
    const float binHz = static_cast<float>(sampleRate) / static_cast<float>(F::kFftSize);

    if (sampleRate != bandSampleRate_) {
        bandSampleRate_ = sampleRate;
        const float ratio = std::pow(kHighestBandHz / kLowestBandHz, 1.0f / static_cast<float>(F::kBandCount));
        for (std::size_t b = 0; b < F::kBandCount; ++b) {
            const float lowHz = kLowestBandHz * std::pow(ratio, static_cast<float>(b));
            const float highHz = lowHz * ratio;
            bandCenterHz_[b] = std::sqrt(lowHz * highHz);

            // The lowest bands are narrower than one FFT bin; give each at least one.
            auto first = static_cast<std::size_t>(std::lround(lowHz / binHz));
            auto last = static_cast<std::size_t>(std::lround(highHz / binHz)) - 1;
            first = std::clamp<std::size_t>(first, 1, F::kSpectrumSize);
            last = std::clamp<std::size_t>(last, first, F::kSpectrumSize);
            bandFirstBin_[b] = first;
            bandLastBin_[b] = last;
        }
    }

    const float bandDecay = decayFactor(dt, 0.12f);
    const float rangeDecay = decayFactor(dt, 0.2f);
    float bass = 0.0f, mid = 0.0f, treble = 0.0f;
    int bassCount = 0, midCount = 0, trebleCount = 0;

    for (std::size_t b = 0; b < F::kBandCount; ++b) {
        float peak = 0.0f;
        for (std::size_t k = bandFirstBin_[b]; k <= bandLastBin_[b]; ++k)
            peak = std::max(peak, magnitude_[k]);

        const float value = toUnit(peak, bandCenterHz_[b]);
        riseFastFallSlow(features_.bands[b], value, bandDecay);

        if (bandCenterHz_[b] < 250.0f)       { bass += value;   ++bassCount; }
        else if (bandCenterHz_[b] < 4000.0f) { mid += value;    ++midCount; }
        else                                 { treble += value; ++trebleCount; }
    }

    riseFastFallSlow(features_.bass, bassCount ? bass / static_cast<float>(bassCount) : 0.0f, rangeDecay);
    riseFastFallSlow(features_.mid, midCount ? mid / static_cast<float>(midCount) : 0.0f, rangeDecay);
    riseFastFallSlow(features_.treble, trebleCount ? treble / static_cast<float>(trebleCount) : 0.0f, rangeDecay);
}

// A beat is a sudden jump in kick-drum-range energy well above its recent average.
void AudioAnalyzer::detectBeat(unsigned sampleRate, float dt) {
    const float binHz = static_cast<float>(sampleRate) / static_cast<float>(F::kFftSize);
    const auto first = static_cast<std::size_t>(std::max(1L, std::lround(40.0f / binHz)));
    const auto last = static_cast<std::size_t>(std::lround(150.0f / binHz));

    float energy = 0.0f;
    for (std::size_t k = first; k <= last; ++k)
        energy += magnitude_[k] * magnitude_[k];

    sinceLastBeat_ += dt;
    const bool loudEnough = energy > 1e-5f;
    const bool spike = energy > bassEnergyAverage_ * 1.5f;
    const bool refractory = sinceLastBeat_ < 0.28f;  // ignore re-triggers within the same hit (~214 BPM cap)

    features_.beat = loudEnough && spike && !refractory;
    if (features_.beat) {
        sinceLastBeat_ = 0.0f;
        features_.beatPulse = 1.0f;
    } else {
        features_.beatPulse *= decayFactor(dt, 0.08f);
    }

    // ~1 second memory of what "normal" bass energy is for this part of the song.
    const float follow = 1.0f - std::exp(-dt / 1.0f);
    bassEnergyAverage_ += (energy - bassEnergyAverage_) * follow;
}
