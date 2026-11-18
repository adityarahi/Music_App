#pragma once

#include <filesystem>
#include <string>

// The app keeps every path as UTF-8 std::string. std::filesystem would read a
// plain std::string in the local ANSI code page on Windows, mangling non-English
// names, so conversions go through char8_t explicitly.

inline std::filesystem::path utf8ToPath(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

inline std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}
