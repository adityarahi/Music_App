#include "App.h"

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

// argv on Windows is in the local ANSI code page; fetch the real UTF-16
// command line so file names with non-English characters survive.
static std::string firstArgUtf8() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::string result;
    if (argv && argc > 1) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, nullptr, 0, nullptr, nullptr);
        result.resize(static_cast<std::size_t>(len > 0 ? len - 1 : 0));
        WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, result.data(), len, nullptr, nullptr);
    }
    LocalFree(argv);
    return result;
}
#endif

int main(int argc, char** argv) {
    // Log lines should appear immediately, even when stdout is a pipe.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

#ifdef _WIN32
    (void)argc;
    (void)argv;
    const std::string file = firstArgUtf8();
#else
    const std::string file = argc > 1 ? argv[1] : "";
#endif

    App app;
    return app.run(file);
}
