// Everything the application knows, in one plain struct the interface reads
// and the tests can build without a window.
#pragma once

#include "app/Settings.h"
#include "app/Tab.h"
#include "platform/FileSystem.h"

#include <string>
#include <vector>

namespace app
{

// Cut/copy live in the application, not the OS clipboard: the OS clipboard
// carries CF_HDROP which needs COM and a window, and a private clipboard is
// enough to move files around inside this explorer.
struct Clipboard
{
    std::vector<std::string> paths;
    bool cut = false;
};

struct AppState
{
    Settings    settings;
    std::string exeDir;          // resolved once at startup
    std::string settingsFile;
    std::string layoutFile;      // imgui.ini beside the exe

    std::vector<Tab> tabs;
    int activeTab = 0;
    int nextTabId = 1;

    std::string buildInfo;       // "SDL 3.4.8, MSVC 1950", filled in by the host for the About box

    std::vector<platform::KnownFolder> quickAccess;
    std::vector<platform::DriveInfo>   drives;

    Clipboard   clipboard;
    std::string statusMessage;   // transient note for the status bar
    bool quitRequested = false;

    Tab&       active();
    const Tab& active() const;
};

// Loads settings, discovers drives and known folders, opens the first tab.
void InitAppState(AppState& state, const std::string& exeDir);

// Writes settings; the last path of the active tab is remembered when the
// user asked for that.
void SaveAppState(AppState& state);

// Returns the index of the new tab, made active.
int  OpenTab(AppState& state, const std::string& path);
void CloseTab(AppState& state, int index);

// Once per frame: reloads any tab that asked for it.
void TickAppState(AppState& state);

// Display name for a location: drive label for roots, folder name otherwise.
std::string LocationTitle(const AppState& state, const std::string& path);

} // namespace app
