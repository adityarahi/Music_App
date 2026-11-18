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

Move the mouse to show the on-screen controls (play/pause, previous/next,
seek bar, volume, visualizer switcher, playlist). They fade out after 3
seconds without mouse movement.

Add music by dragging files or folders onto the window, passing them on the
command line (`music_app song1.mp3 song2.mp3 ~/Music`), or with **Add files...**
in the playlist. MP3, FLAC and WAV are supported.

| Key | Action |
|---|---|
| Space | Play / pause |
| Left / Right | Seek 5 s (hold Shift for 30 s) |
| Up / Down, mouse wheel | Volume |
| N / Shift+N | Next / previous track |
| V / Shift+V | Next / previous visualizer (1.5 s cross-fade) |
| P | Show / hide playlist |
| O | Add files... |
| Tab | Hide / show all controls |
| Esc | Quit |

In the playlist: double-click a track to play it, right-click to remove it.

## Layout

```
src/
  main.cpp              entry point (UTF-8 command line on Windows)
  App.*                 window, main loop, input
  DevTour.*             screenshot tour for checking visuals (see below)
  audio/
    AudioEngine.*       miniaudio playback; taps mixed audio on the audio thread
    RingBuffer.h        lock-free audio -> render hand-off
    AudioAnalyzer.*     per-frame features: waveform, FFT spectrum, 32 log bands,
                        bass/mid/treble, loudness, beat detection
  player/
    Playlist.*          track list, current track, repeat
    Player.*            transport logic shared by keys, mouse and drag & drop
    Paths.h             UTF-8 <-> std::filesystem conversions
  ui/
    Ui.*                Dear ImGui control bar, playlist panel, auto-hide
    FileBrowser.*       built-in "Add music" file picker
  render/
    Shader.*            GLSL program helper
    RenderTarget.*      offscreen framebuffer + FramebufferScope
    Fullscreen.*        full-screen triangle for post-processing passes
    GlowLine.*          thick soft-edged glowing polylines
    Color.h             HSV -> RGB
  viz/
    Visualizer.h        interface every effect implements
    VisualizerManager.* switching and cross-fades
    SpectrumBars.*      glowing bars with falling peak caps and reflection
    FeedbackTunnel.*    waveform ring with zooming, fading feedback trails
    Plasma.*            shader-only flowing colour field
    Oscilloscope.*      stereo glowing waveform
third_party/glad/       generated OpenGL 3.3 core loader
cmake/                  MinGW cross-compile toolchain
```

## Screenshot tour

Set `MUSIC_APP_TOUR` to a directory and the app seeks to 1:00, saves a
screenshot of the controls with the playlist open, then shows each visualizer
for a few seconds, saves one `.ppm` screenshot per visualizer (plus one mid
cross-fade), and exits:

```bash
mkdir -p shots && MUSIC_APP_TOUR=shots ./build/linux-debug/music_app song.mp3
# Windows build from WSL: pass the variable through with WSLENV
MUSIC_APP_TOUR=shots WSLENV=MUSIC_APP_TOUR/p ./build/windows-release/music_app.exe song.mp3
```
