#pragma once

#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

// Operating-system integration. Implemented for Windows; elsewhere these are
// harmless no-ops so the rest of the app doesn't need #ifdefs.
namespace platform {

// --- Single instance -------------------------------------------------------
// When the app is already running, a second launch (e.g. double-clicking an
// MP3 in Explorer) hands its files to the running window and exits.

// Call at startup, before creating a window. Returns true if another instance
// is running and has received `paths` (and was brought to the front); the
// caller should then exit.
bool forwardToRunningInstance(const std::vector<std::string>& paths);

// Call once the main window exists: marks it as the window to forward to and
// delivers files sent by later launches to `onFiles` (on the main thread,
// during glfwPollEvents).
void receiveForwardedFiles(GLFWwindow* window, std::function<void(std::vector<std::string>)> onFiles);

// --- "Open with" in the file manager ---------------------------------------

bool supportsOpenWith();
bool isOpenWithRegistered();
// Registers this executable for .mp3/.flac/.wav under the current user only
// (no administrator rights needed). Returns false and fills `error` on failure.
bool registerOpenWith(std::string& error);
void unregisterOpenWith();

// UTF-8 path of the running executable.
std::string executablePath();

}  // namespace platform
