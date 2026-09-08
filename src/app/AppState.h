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

// Cut/copy go to the Windows clipboard so they interoperate with Explorer.
// This is a mirror of what THIS app last placed there, kept so cut items
// can be drawn ghosted; it is dropped as soon as anything else writes the
// clipboard.
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

    Clipboard     clipboard;
    unsigned long clipboardOwnedSeq = 0;   // clipboard sequence when `clipboard` was written
    bool          clipboardHasFiles = false;

    std::string statusMessage;   // transient note for the status bar
    bool quitRequested = false;

    Tab&       active();
    const Tab& active() const;
};

// Loads settings, discovers drives and known folders, opens the first tab.
void InitAppState(AppState& state, const std::string& exeDir, const std::string& configDir = {});

// A path handed over on the command line (by the shell, a shortcut, or a
// terminal) replaces the first tab: a folder opens directly, a file opens
// its folder with the file selected, and anything else (a shell CLSID
// string, garbage) lands on This PC. Returns false for that last case.
bool ApplyStartArgument(AppState& state, const std::string& argument);

// Writes settings; the last path of the active tab is remembered when the
// user asked for that.
void SaveAppState(AppState& state);

// Returns the index of the new tab, made active.
int  OpenTab(AppState& state, const std::string& path);
void CloseTab(AppState& state, int index);

// Once per frame: reloads any tab that asked for it, syncs clipboard facts.
void TickAppState(AppState& state);

// After a file operation whose reach is unknown: every tab re-reads.
void RefreshAll(AppState& state);

// Display name for a location: drive label for roots, folder name otherwise.
std::string LocationTitle(const AppState& state, const std::string& path);

} // namespace app
