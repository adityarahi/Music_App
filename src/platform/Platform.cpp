#include "platform/Platform.h"

#ifndef _WIN32

namespace platform {

bool forwardToRunningInstance(const std::vector<std::string>&) { return false; }
void receiveForwardedFiles(GLFWwindow*, std::function<void(std::vector<std::string>)>) {}
bool supportsOpenWith() { return false; }
bool isOpenWithRegistered() { return false; }
bool registerOpenWith(std::string& error) {
    error = "Only available on Windows.";
    return false;
}
void unregisterOpenWith() {}
std::string executablePath() { return {}; }

}  // namespace platform

#else

#include <windows.h>
#include <shlobj.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <chrono>
#include <thread>

namespace platform {

namespace {

constexpr wchar_t kMutexName[] = L"Local\\Dhundarsh.SingleInstance";
constexpr wchar_t kWindowProp[] = L"Dhundarsh.MainWindow";
constexpr ULONG_PTR kCopyDataFiles = 0x4D41;   // tags our WM_COPYDATA messages
constexpr wchar_t kExeName[] = L"dhundarsh.exe";
constexpr wchar_t kAppKey[] = L"Software\\Classes\\Applications\\dhundarsh.exe";
constexpr const wchar_t* kExtensions[] = {L".mp3", L".flac", L".wav"};
// Registered by builds from before the app was renamed; cleaned up so
// Explorer doesn't keep offering the old name.
constexpr wchar_t kLegacyExeName[] = L"music_app.exe";

std::string narrow(const std::wstring& s) {
    if (s.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring exePathW() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n < path.size()) {
            path.resize(n);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

BOOL CALLBACK findMainWindow(HWND hwnd, LPARAM result) {
    if (GetPropW(hwnd, kWindowProp)) {
        *reinterpret_cast<HWND*>(result) = hwnd;
        return FALSE;
    }
    return TRUE;
}

// Paths are sent as UTF-8 joined with '\0'.
std::vector<std::string> unpackPaths(const char* data, std::size_t size) {
    std::vector<std::string> paths;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= size; ++i) {
        if (i == size || data[i] == '\0') {
            if (i > start) paths.emplace_back(data + start, i - start);
            start = i + 1;
        }
    }
    return paths;
}

// State for the window procedure hook (one main window per process).
WNDPROC g_previousProc = nullptr;
std::function<void(std::vector<std::string>)> g_onFiles;

LRESULT CALLBACK hookedWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COPYDATA) {
        const auto* copy = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (copy && copy->dwData == kCopyDataFiles) {
            if (g_onFiles) g_onFiles(unpackPaths(static_cast<const char*>(copy->lpData), copy->cbData));
            return TRUE;
        }
    }
    return CallWindowProcW(g_previousProc, hwnd, msg, wParam, lParam);
}

void removeRegistration(const std::wstring& exeName) {
    RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\Applications\\" + exeName).c_str());
    for (const wchar_t* ext : kExtensions)
        RegDeleteTreeW(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + ext + L"\\OpenWithList\\" + exeName).c_str());
}

bool setString(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& value) {
    HKEY h;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS) return false;
    const LONG rc = RegSetValueExW(h, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return rc == ERROR_SUCCESS;
}

}  // namespace

bool forwardToRunningInstance(const std::vector<std::string>& paths) {
    // The mutex is held for the life of the first process; later launches see it already exists.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    CloseHandle(mutex);

    // Explorer may launch several copies at once (one per selected file); give
    // the first one a moment to open its window.
    HWND target = nullptr;
    for (int attempt = 0; attempt < 50 && !target; ++attempt) {
        EnumWindows(findMainWindow, reinterpret_cast<LPARAM>(&target));
        if (!target) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!target) return false;   // couldn't find it; just run as a normal second instance

    std::string packed;
    for (const auto& p : paths) {
        packed += p;
        packed += '\0';
    }
    COPYDATASTRUCT copy{kCopyDataFiles, static_cast<DWORD>(packed.size()), packed.data()};
    DWORD_PTR result = 0;
    SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copy), SMTO_ABORTIFHUNG, 3000, &result);

    if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
    SetForegroundWindow(target);
    return true;
}

void receiveForwardedFiles(GLFWwindow* window, std::function<void(std::vector<std::string>)> onFiles) {
    HWND hwnd = glfwGetWin32Window(window);
    SetPropW(hwnd, kWindowProp, reinterpret_cast<HANDLE>(1));
    g_onFiles = std::move(onFiles);
    g_previousProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hookedWindowProc)));
}

bool supportsOpenWith() { return true; }

bool isOpenWithRegistered() {
    HKEY h;
    const std::wstring key = std::wstring(kAppKey) + L"\\shell\\open\\command";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) return false;

    // Registered, but for this copy of the exe? (It may have been moved.)
    wchar_t value[2048] = {};
    DWORD size = sizeof(value) - sizeof(wchar_t);
    const bool ok = RegQueryValueExW(h, nullptr, nullptr, nullptr, reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS;
    RegCloseKey(h);
    return ok && std::wstring(value).find(exePathW()) != std::wstring::npos;
}

bool registerOpenWith(std::string& error) {
    removeRegistration(kLegacyExeName);

    const std::wstring exe = exePathW();
    const std::wstring app = kAppKey;
    bool ok = setString(HKEY_CURRENT_USER, app + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
    ok = ok && setString(HKEY_CURRENT_USER, app, L"FriendlyAppName", L"Dhundarsh");
    ok = ok && setString(HKEY_CURRENT_USER, app + L"\\DefaultIcon", nullptr, L"\"" + exe + L"\",0");
    for (const wchar_t* ext : kExtensions) {
        ok = ok && setString(HKEY_CURRENT_USER, app + L"\\SupportedTypes", ext, L"");
        ok = ok && setString(HKEY_CURRENT_USER, std::wstring(L"Software\\Classes\\") + ext + L"\\OpenWithList\\" + kExeName, nullptr, L"");
    }
    if (!ok) {
        error = "Could not write to the registry.";
        return false;
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

void unregisterOpenWith() {
    removeRegistration(kExeName);
    removeRegistration(kLegacyExeName);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

std::string executablePath() { return narrow(exePathW()); }

}  // namespace platform

#endif
