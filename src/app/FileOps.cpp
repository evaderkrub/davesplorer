#include "app/FileOps.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "platform/FileSystem.h"
#include "platform/Shell.h"
#include "platform/Strings.h"

namespace app
{

namespace
{

bool NameExists(const Tab& tab, const std::string& name)
{
    const std::string lower = platform::ToLowerAscii(name);
    for (const platform::FileEntry& e : tab.entries)
        if (platform::ToLowerAscii(e.name) == lower) return true;
    return false;
}

} // namespace

bool OpenEntry(AppState& state, Tab& tab, int entryIndex, std::string& error)
{
    (void)state;
    if (entryIndex < 0 || entryIndex >= (int)tab.entries.size()) return false;
    const platform::FileEntry& e = tab.entries[(size_t)entryIndex];
    if (e.isDirectory) return NavigateTo(tab, e.path, error);
    return platform::OpenWithShell(e.path, error);
}

std::string UniqueNewName(const Tab& tab, const std::string& base)
{
    if (!NameExists(tab, base)) return base;
    // Explorer keeps the extension on the outside: "New file (2).txt".
    const size_t dot = base.find_last_of('.');
    const std::string stem = (dot == std::string::npos || dot == 0) ? base : base.substr(0, dot);
    const std::string ext  = (dot == std::string::npos || dot == 0) ? "" : base.substr(dot);
    for (int n = 2; n < 10000; ++n)
        {
        const std::string candidate = stem + " (" + std::to_string(n) + ")" + ext;
        if (!NameExists(tab, candidate)) return candidate;
        }
    return base;
}

bool CreateFolderIn(Tab& tab, const std::string& name, std::string& error)
{
    if (tab.path.empty())
        {
        error = "Folders can't be created in This PC.";
        return false;
        }
    if (!IsValidFileName(name, error)) return false;
    if (!platform::CreateFolder(JoinPath(tab.path, name), error)) return false;
    Refresh(tab);
    return true;
}

bool CreateFileIn(Tab& tab, const std::string& name, std::string& error)
{
    if (tab.path.empty())
        {
        error = "Files can't be created in This PC.";
        return false;
        }
    if (!IsValidFileName(name, error)) return false;
    if (!platform::CreateEmptyFile(JoinPath(tab.path, name), error)) return false;
    Refresh(tab);
    return true;
}

bool RenameEntry(Tab& tab, int entryIndex, const std::string& newName, std::string& error)
{
    if (entryIndex < 0 || entryIndex >= (int)tab.entries.size()) return false;
    if (tab.path.empty())
        {
        error = "Drives can't be renamed here.";
        return false;
        }
    if (!IsValidFileName(newName, error)) return false;
    const platform::FileEntry& e = tab.entries[(size_t)entryIndex];
    if (newName == e.name) return true;
    if (!platform::RenamePath(e.path, JoinPath(tab.path, newName), error)) return false;
    Refresh(tab);
    return true;
}

bool DeleteSelected(Tab& tab, bool permanent, std::string& error)
{
    if (tab.path.empty())
        {
        error = "Drives can't be deleted.";
        return false;
        }
    const std::vector<std::string> paths = SelectedPaths(tab);
    if (paths.empty()) return true;
    const bool ok = permanent ? platform::DeletePermanently(paths, error)
                              : platform::MoveToRecycleBin(paths, error);
    // Even a partial failure changes the folder, so reload either way.
    Refresh(tab);
    return ok;
}

void CopySelection(AppState& state, Tab& tab, bool cut)
{
    if (tab.path.empty()) return;
    state.clipboard.paths = SelectedPaths(tab);
    state.clipboard.cut = cut;
}

bool CanPaste(const AppState& state, const Tab& tab)
{
    return !state.clipboard.paths.empty() && !tab.path.empty();
}

bool Paste(AppState& state, Tab& tab, std::string& error)
{
    if (!CanPaste(state, tab)) return false;
    bool sameFolder = true;
    for (const std::string& p : state.clipboard.paths)
        if (ParentPath(p) != tab.path) sameFolder = false;

    bool ok;
    if (state.clipboard.cut)
        {
        if (sameFolder) return true;   // moving a file onto itself is a no-op
        ok = platform::MovePaths(state.clipboard.paths, tab.path, error);
        // A cut is one-shot; a second paste must not move the files again.
        if (ok) state.clipboard = Clipboard{};
        }
    else
        {
        ok = platform::CopyPaths(state.clipboard.paths, tab.path, sameFolder, error);
        }
    Refresh(tab);
    return ok;
}

} // namespace app
