#include "audio/AudioEngine.h"

#include <miniaudio.h>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}
#endif

}  // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init() {
    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;  // the ring buffer and visualizers assume stereo
    config.onProcess = &AudioEngine::onAudioProcessed;
    config.pProcessUserData = this;

    engine_ = std::make_unique<ma_engine>();
    const ma_result result = ma_engine_init(&config, engine_.get());
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "Audio: failed to initialise engine: %s\n", ma_result_description(result));
        engine_.reset();
        return false;
    }

    const ma_backend backend = ma_engine_get_device(engine_.get())->pContext->backend;
    std::printf("Audio: %s, %u Hz, %u channels\n", ma_get_backend_name(backend), sampleRate(),
                ma_engine_get_channels(engine_.get()));

    // miniaudio loads system audio libraries at runtime and quietly falls back
    // to a silent "null" device if none are found.
    if (backend == ma_backend_null) {
        std::fprintf(stderr,
                     "Audio: WARNING: no real audio output found; playback will be silent.\n"
#ifdef __linux__
                     "       Install the PulseAudio client library: sudo apt install libpulse0\n"
#endif
        );
    }
    return true;
}

void AudioEngine::shutdown() {
    unload();
    if (engine_) {
        ma_engine_uninit(engine_.get());
        engine_.reset();
    }
}

bool AudioEngine::load(const std::string& path) {
    if (!engine_) return false;
    unload();

    // STREAM: decode incrementally on miniaudio's job thread instead of
    // loading the whole file up front, so the audio thread only copies.
    const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;

    auto sound = std::make_unique<ma_sound>();
#ifdef _WIN32
    const std::wstring widePath = utf8ToWide(path);
    const ma_result result = ma_sound_init_from_file_w(engine_.get(), widePath.c_str(), flags, nullptr, nullptr, sound.get());
#else
    const ma_result result = ma_sound_init_from_file(engine_.get(), path.c_str(), flags, nullptr, nullptr, sound.get());
#endif
    if (result != MA_SUCCESS) {
        std::fprintf(stderr, "Audio: cannot open '%s': %s\n", path.c_str(), ma_result_description(result));
        return false;
    }

    sound_ = std::move(sound);
    play();
    return true;
}

void AudioEngine::unload() {
    if (sound_) {
        ma_sound_uninit(sound_.get());
        sound_.reset();
    }
}

void AudioEngine::play() {
    if (!sound_) return;
    if (isAtEnd()) ma_sound_seek_to_pcm_frame(sound_.get(), 0);
    ma_sound_start(sound_.get());
}

void AudioEngine::pause() {
    if (sound_) ma_sound_stop(sound_.get());
}

void AudioEngine::togglePause() {
    if (isPlaying()) pause();
    else play();
}

void AudioEngine::seek(double seconds) {
    if (!sound_) return;
    const double len = duration();
    if (seconds < 0.0) seconds = 0.0;
    if (len > 0.0 && seconds > len) seconds = len;
    ma_sound_seek_to_second(sound_.get(), static_cast<float>(seconds));
}

bool AudioEngine::isPlaying() const {
    return sound_ && ma_sound_is_playing(sound_.get());
}

bool AudioEngine::isAtEnd() const {
    return sound_ && ma_sound_at_end(sound_.get());
}

double AudioEngine::position() const {
    if (!sound_) return 0.0;
    float cursor = 0.0f;
    ma_sound_get_cursor_in_seconds(sound_.get(), &cursor);
    return cursor;
}

double AudioEngine::duration() const {
    if (!sound_) return 0.0;
    float length = 0.0f;
    ma_sound_get_length_in_seconds(sound_.get(), &length);
    return length;
}

void AudioEngine::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    volume_.store(volume, std::memory_order_relaxed);
}

unsigned AudioEngine::sampleRate() const {
    return engine_ ? ma_engine_get_sample_rate(engine_.get()) : 0;
}

// Runs on the audio device thread after miniaudio has mixed a block.
// Must stay wait-free: no locks, allocation, I/O or logging.
void AudioEngine::onAudioProcessed(void* userData, float* frames, unsigned long long frameCount) {
    auto* self = static_cast<AudioEngine*>(userData);

    self->samples_.write(frames, static_cast<std::size_t>(frameCount));

    const float vol = self->volume_.load(std::memory_order_relaxed);
    const unsigned long long sampleCount = frameCount * 2;
    for (unsigned long long i = 0; i < sampleCount; ++i)
        frames[i] *= vol;
}
