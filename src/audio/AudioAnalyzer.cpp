#include "audio/AudioAnalyzer.h"

#include <algorithm>
#include <cmath>

void AudioAnalyzer::update(const AudioEngine::SampleBuffer& samples, float dt) {
    AudioFeatures& f = features_;
    f.time += dt;

    samples.readLatest(f.waveformLeft.data(), f.waveformRight.data(), AudioFeatures::kWaveformSize);

    double sumSquares = 0.0;
    for (std::size_t i = 0; i < AudioFeatures::kWaveformSize; ++i) {
        const float mono = 0.5f * (f.waveformLeft[i] + f.waveformRight[i]);
        sumSquares += static_cast<double>(mono) * mono;
    }
    f.rms = static_cast<float>(std::sqrt(sumSquares / AudioFeatures::kWaveformSize));

    // Rise instantly, fall off exponentially (~halves every 0.15 s). Frame-rate
    // independent so it looks the same at 60 Hz and 144 Hz.
    const float decay = std::exp2(-dt / 0.15f);
    f.level = std::max(f.rms, f.level * decay);
}
