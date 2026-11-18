cmake_minimum_required(VERSION 3.24)
project(music_app VERSION 0.1.0 LANGUAGES C CXX)

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

FetchContent_MakeAvailable(glfw miniaudio)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})

# glad: OpenGL 3.3 core loader, pre-generated into third_party/glad.
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

add_executable(music_app
    src/main.cpp
    src/App.cpp
    src/audio/AudioEngine.cpp
    src/audio/AudioAnalyzer.cpp
    src/audio/miniaudio_impl.c
    src/render/Shader.cpp
    src/viz/Oscilloscope.cpp
)

target_include_directories(music_app PRIVATE src)
target_link_libraries(music_app PRIVATE glfw glad miniaudio)

if(MSVC)
    target_compile_options(music_app PRIVATE /W4)
else()
    target_compile_options(music_app PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>)
endif()

if(WIN32)
    target_compile_definitions(music_app PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(music_app PRIVATE shell32)
    if(MINGW)
        # Single self-contained .exe: no libstdc++/libgcc/winpthread DLLs to ship.
        target_link_options(music_app PRIVATE -static)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(music_app PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
