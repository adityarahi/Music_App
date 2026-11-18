cmake_minimum_required(VERSION 3.24)
project(dhundarsh VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------

# GLFW: window, input, OpenGL context. X11 only on Linux (works under WSLg).
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9
)

# miniaudio: MP3 decoding + audio output (WASAPI on Windows, PulseAudio/ALSA on Linux).
# Header-only; SOURCE_SUBDIR points nowhere so its own CMakeLists is not pulled in.
FetchContent_Declare(miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.22.tar.gz
    SOURCE_SUBDIR _header_only
)

# KissFFT: small real-input FFT for the spectrum analyzer (float build).
# Its own CMakeLists is skipped; the two source files we need are compiled below.
FetchContent_Declare(kissfft
    URL https://github.com/mborgerding/kissfft/archive/refs/tags/131.2.0.tar.gz
    URL_HASH SHA256=205a8f6a448ef12b091f8ac6a514b5091bb5f6b0b543431ed75f673116cf5cbf
    SOURCE_SUBDIR _sources_only
)

# Dear ImGui: on-screen controls drawn over the visualizer.
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
    SOURCE_SUBDIR _sources_only
)

FetchContent_MakeAvailable(glfw miniaudio kissfft imgui)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})

add_library(kissfft STATIC
    ${kissfft_SOURCE_DIR}/kiss_fft.c
    ${kissfft_SOURCE_DIR}/kiss_fftr.c
)
target_include_directories(kissfft SYSTEM PUBLIC ${kissfft_SOURCE_DIR})

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# glad: OpenGL 3.3 core loader, pre-generated into third_party/glad.
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

add_executable(dhundarsh
    src/main.cpp
    src/App.cpp
    src/platform/Platform.cpp
    src/DevTour.cpp
    src/audio/AudioEngine.cpp
    src/audio/AudioAnalyzer.cpp
    src/audio/miniaudio_impl.c
    src/player/Player.cpp
    src/player/Playlist.cpp
    src/ui/FileBrowser.cpp
    src/ui/Ui.cpp
    src/render/ColorMesh.cpp
    src/render/Fullscreen.cpp
    src/render/GlowLine.cpp
    src/render/PointSprites.cpp
    src/render/RenderTarget.cpp
    src/render/Shader.cpp
    src/viz/CircularSpectrum.cpp
    src/viz/FeedbackTunnel.cpp
    src/viz/Kaleidoscope.cpp
    src/viz/Oscilloscope.cpp
    src/viz/Plasma.cpp
    src/viz/SpectrumBars.cpp
    src/viz/VisualizerManager.cpp
)

target_include_directories(dhundarsh PRIVATE src)
target_link_libraries(dhundarsh PRIVATE glfw glad miniaudio kissfft imgui)

if(MSVC)
    target_compile_options(dhundarsh PRIVATE /W4)
else()
    target_compile_options(dhundarsh PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>)
endif()

if(WIN32)
    target_compile_definitions(dhundarsh PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(dhundarsh PRIVATE shell32 advapi32)

    # Icon + version info (the version's FileDescription is the name shown in "Open with").
    enable_language(RC)
    set(ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets/dhundarsh.ico)
    configure_file(cmake/dhundarsh.rc.in ${CMAKE_CURRENT_BINARY_DIR}/dhundarsh.rc @ONLY)
    target_sources(dhundarsh PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dhundarsh.rc)

    # Release builds are GUI apps with no console window; Debug keeps the console for logs.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(dhundarsh PROPERTIES WIN32_EXECUTABLE ON)
    endif()
    if(MINGW)
        # Single self-contained .exe: no libstdc++/libgcc/winpthread DLLs to ship.
        target_link_options(dhundarsh PRIVATE -static)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(dhundarsh PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
