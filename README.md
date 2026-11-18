# Music App

An MP3 player with real-time OpenGL visualizations. C++20, GLFW, miniaudio, OpenGL 3.3 core.

## Building (from WSL / Ubuntu)

One-time setup:

```bash
sudo apt install -y cmake ninja-build mingw-w64 \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev \
    libpulse0
```

`libpulse0` is needed for sound on Linux/WSLg. Without it the app still runs but
logs `Audio: Null` and plays silently.

Linux build (runs in a WSLg window):

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
./build/linux-debug/music_app path/to/song.mp3
```

Windows build (cross-compiled with MinGW-w64, produces a self-contained `.exe`):

```bash
cmake --preset windows-release
cmake --build --preset windows-release
./build/windows-release/music_app.exe     # WSL launches it as a native Windows app
```

The Windows `.exe` receives paths as Windows sees them, so pass a relative path
or a `C:\...` path rather than `/home/...`, or just drag a file onto the window.

## Controls

| Key | Action |
|---|---|
| Drag & drop | Open an MP3 |
| Space | Play / pause |
| Left / Right | Seek 5 s (hold Shift for 30 s) |
| Up / Down | Volume |
| V | Next visualizer |
| Esc | Quit |

## Layout

```
src/
  main.cpp              entry point (UTF-8 command line on Windows)
  App.*                 window, main loop, input
  audio/
    AudioEngine.*       miniaudio playback; taps mixed audio on the audio thread
    RingBuffer.h        lock-free audio -> render hand-off
    AudioAnalyzer.*     per-frame features: waveform, FFT spectrum, 32 log bands,
                        bass/mid/treble, loudness, beat detection
  render/
    Shader.*            GLSL program helper
    Color.h             HSV -> RGB
  viz/
    Visualizer.h        interface every effect implements
    SpectrumBars.*      glowing bars with falling peak caps and reflection
    Oscilloscope.*      stereo glowing waveform
third_party/glad/       generated OpenGL 3.3 core loader
cmake/                  MinGW cross-compile toolchain
```
