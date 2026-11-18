#pragma once

#include "audio/RingBuffer.h"

#include <atomic>
#include <memory>
#include <string>

struct ma_engine;
struct ma_sound;

// Owns audio output and the currently playing track.
//
// Threading: every public method is called from the main thread. miniaudio's
// own threads decode the MP3 (resource manager job thread) and mix it (device
// thread). The device thread hands each mixed block to onAudioProcessed(),
// which copies it into `samples()` for the visualizers.
class AudioEngine {
public:
    using SampleBuffer = StereoRingBuffer<16384>;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool init();
    void shutdown();

    // `path` is UTF-8 on every platform. Starts playback on success.
    bool load(const std::string& path);
    void unload();

    void play();
    void pause();
    void togglePause();
    void seek(double seconds);

    bool hasTrack() const { return sound_ != nullptr; }
    bool isPlaying() const;
    bool isAtEnd() const;
    double position() const;
    double duration() const;

    // 0..1. Applied after the visualizer tap, so visuals don't shrink when
    // the music is turned down.
    void setVolume(float volume);
    float volume() const { return volume_.load(std::memory_order_relaxed); }

    unsigned sampleRate() const;
    const SampleBuffer& samples() const { return samples_; }

private:
    static void onAudioProcessed(void* userData, float* frames, unsigned long long frameCount);

    std::unique_ptr<ma_engine> engine_;
    std::unique_ptr<ma_sound> sound_;
    std::atomic<float> volume_{0.8f};
    SampleBuffer samples_;
};
