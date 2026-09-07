#include "app/AppState.h"
#include "app/Listing.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "platform/Clipboard.h"
#include "platform/Strings.h"

#include <algorithm>
#include <cstring>

namespace app
{

Tab& AppState::active()
{
    if (tabs.empty()) tabs.emplace_back();
    activeTab = std::clamp(activeTab, 0, (int)tabs.size() - 1);
    return tabs[(size_t)activeTab];
}

const Tab& AppState::active() const
{
    const int i = std::clamp(activeTab, 0, (int)tabs.size() - 1);
    return tabs[(size_t)i];
}

void InitAppState(AppState& state, const std::string& exeDir)
{
    state.exeDir = exeDir;
    state.settingsFile = JoinPath(exeDir, "settings.ini");
    state.layoutFile   = JoinPath(exeDir, "imgui.ini");
    LoadSettings(state.settingsFile, state.settings);

    state.quickAccess = platform::GetKnownFolders();
    state.drives      = platform::GetDrives();

    std::string start = state.settings.startPath;
    if (!start.empty() && !platform::IsDirectory(start)) start.clear();
    if (start.empty())
        {
        // First run, or the remembered folder is gone: the user's home is a
        // more useful place than This PC.
        for (const platform::KnownFolder& kf : state.quickAccess)
            if (kf.kind == platform::KnownFolderKind::Home) start = kf.path;
        }
    state.tabs.clear();
    OpenTab(state, start);
}

bool ApplyStartArgument(AppState& state, const std::string& argument)
{
    std::string raw = argument;
    // Explorer's own switches, so a program that runs
    // `explorer.exe /select,"file"` or `/e,folder` can run davesplorer.exe
    // with the same arguments unchanged. /select on a file selects it;
    // on a folder it shows the parent with the folder selected.
    bool selectMode = false;
    for (const char* prefix : { "/select,", "/e,", "/root," })
        {
        const size_t n = strlen(prefix);
        if (raw.size() >= n && platform::ToLowerAscii(raw.substr(0, n)) == prefix)
            {
            selectMode = prefix[1] == 's';
            raw = raw.substr(n);
            break;
            }
        }
    std::string path = NormalizePath(raw);
    // Explorer quotes paths it passes along; a shell verb may leave the
    // quotes in.
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = NormalizePath(path.substr(1, path.size() - 2));

    std::string folder, select;
    if (!path.empty() && platform::IsDirectory(path) && !(selectMode && !IsDriveRoot(path)))
        {
        folder = path;
        }
    else if (!path.empty() && platform::PathExists(path))
        {
        folder = ParentPath(path);
        select = PathName(path);
        }

    state.tabs.clear();
    state.activeTab = 0;
    OpenTab(state, folder);
    state.active().selectOnLoad = select;
    return !folder.empty();
}

void SaveAppState(AppState& state)
{
    if (state.settings.rememberLastPath && !state.tabs.empty())
        state.settings.startPath = state.active().path;
    std::string error;
    SaveSettings(state.settingsFile, state.settings, error);
}

int OpenTab(AppState& state, const std::string& path)
{
    Tab tab;
    tab.id = state.nextTabId++;
    std::string error;
    if (!NavigateTo(tab, path, error)) NavigateTo(tab, "", error);
    state.tabs.push_back(std::move(tab));
    state.activeTab = (int)state.tabs.size() - 1;
    return state.activeTab;
}

void CloseTab(AppState& state, int index)
{
    if (index < 0 || index >= (int)state.tabs.size()) return;
    if (state.tabs.size() == 1)
        {
        // Explorer closes the window when its last tab goes; so do we.
        state.quitRequested = true;
        return;
        }
    state.tabs.erase(state.tabs.begin() + index);
    if (state.activeTab >= (int)state.tabs.size()) state.activeTab = (int)state.tabs.size() - 1;
    else if (state.activeTab > index) --state.activeTab;
}

void TickAppState(AppState& state)
{
    for (Tab& tab : state.tabs)
        ReloadIfNeeded(tab, state.settings);

    state.clipboardHasFiles = platform::ClipboardHasFiles();
    // Someone else wrote the clipboard: the ghosted cut list no longer
    // describes what a paste would do.
    if (!state.clipboard.paths.empty() && platform::ClipboardSequence() != state.clipboardOwnedSeq)
        state.clipboard = Clipboard{};
}

void RefreshAll(AppState& state)
{
    for (Tab& tab : state.tabs)
        tab.needsReload = true;
}

std::string LocationTitle(const AppState& state, const std::string& path)
{
    if (IsDriveRoot(path))
        for (const platform::DriveInfo& d : state.drives)
            if (d.root == path) return d.displayName;
    return PathName(path);
}

} // namespace app
