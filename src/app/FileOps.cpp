#include "app/FileOps.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "platform/Clipboard.h"
#include "platform/FileSystem.h"
#include "platform/Shell.h"
#include "platform/Strings.h"

#include <cctype>

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

bool CopySelection(AppState& state, Tab& tab, bool cut, std::string& error)
{
    if (tab.path.empty()) return false;
    const std::vector<std::string> paths = SelectedPaths(tab);
    if (paths.empty()) return true;
    if (!platform::SetClipboardFiles(paths, cut, error)) return false;
    state.clipboard.paths = paths;
    state.clipboard.cut = cut;
    state.clipboardOwnedSeq = platform::ClipboardSequence();
    state.clipboardHasFiles = true;
    return true;
}

bool CanPaste(const AppState& state, const Tab& tab)
{
    return state.clipboardHasFiles && !tab.path.empty();
}

bool Paste(AppState& state, Tab& tab, std::string& error)
{
    if (tab.path.empty()) return false;
    std::vector<std::string> paths;
    bool cut = false;
    if (!platform::GetClipboardFiles(paths, cut)) return false;
    const bool ok = DropPaths(state, paths, tab.path, cut ? DropAction::Move : DropAction::Copy, error);
    // A cut is one-shot: Explorer empties the clipboard after the move so
    // a second paste cannot move the files again.
    if (ok && cut)
        {
        platform::ClearClipboard();
        state.clipboard = Clipboard{};
        state.clipboardHasFiles = false;
        }
    return ok;
}

DropAction DefaultDropAction(const std::vector<std::string>& sources, const std::string& destDir, bool ctrl, bool shift)
{
    if (ctrl) return DropAction::Copy;
    if (shift) return DropAction::Move;
    if (sources.empty() || destDir.size() < 2 || sources[0].size() < 2) return DropAction::Copy;
    const bool sameDrive = std::toupper((unsigned char)sources[0][0]) == std::toupper((unsigned char)destDir[0]) &&
                           sources[0][1] == ':' && destDir[1] == ':';
    return sameDrive ? DropAction::Move : DropAction::Copy;
}

bool CanDropOn(const std::vector<std::string>& sources, const std::string& destDir)
{
    if (destDir.empty() || sources.empty()) return false;
    const std::string destLower = platform::ToLowerAscii(destDir);
    for (const std::string& src : sources)
        {
        const std::string srcLower = platform::ToLowerAscii(src);
        if (srcLower == destLower) return false;
        // A folder into its own subtree.
        if (destLower.size() > srcLower.size() && destLower.compare(0, srcLower.size(), srcLower) == 0 &&
            destLower[srcLower.size()] == '\\')
            return false;
        }
    return true;
}

bool DropPaths(AppState& state, const std::vector<std::string>& sources, const std::string& destDir,
               DropAction action, std::string& error)
{
    if (destDir.empty())
        {
        error = "Items can't be dropped on This PC. Choose a folder.";
        return false;
        }
    if (!CanDropOn(sources, destDir))
        {
        error = "The destination folder is the source folder or one of its subfolders.";
        return false;
        }
    bool sameFolder = true;
    for (const std::string& p : sources)
        if (platform::ToLowerAscii(ParentPath(p)) != platform::ToLowerAscii(destDir)) sameFolder = false;

    bool ok = true;
    if (action == DropAction::Move)
        {
        // Moving files onto the folder they are in is a no-op, not an error.
        if (!sameFolder) ok = platform::MovePaths(sources, destDir, error);
        }
    else
        {
        ok = platform::CopyPaths(sources, destDir, sameFolder, error);
        }
    RefreshAll(state);
    return ok;
}

} // namespace app
