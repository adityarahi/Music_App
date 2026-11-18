#include "App.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

// argv on Windows is in the local ANSI code page; fetch the real UTF-16
// command line so file names with non-English characters survive.
static std::vector<std::string> argsUtf8() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> result;
    for (int i = 1; argv && i < argc; ++i) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string arg(static_cast<std::size_t>(len > 0 ? len - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg.data(), len, nullptr, nullptr);
        result.push_back(std::move(arg));
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
    const std::vector<std::string> paths = argsUtf8();
#else
    const std::vector<std::string> paths(argv + 1, argv + argc);
#endif

    App app;
    return app.run(paths);
}
