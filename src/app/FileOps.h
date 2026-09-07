// Operations on the files in a tab. Each returns false with a message the
// interface can show; the tab is asked to reload afterwards so the listing
// reflects what actually happened on disk.
#pragma once

#include "app/AppState.h"

#include <string>
#include <vector>

namespace app
{

// Enter or double-click: folders navigate, files launch.
bool OpenEntry(AppState& state, Tab& tab, int entryIndex, std::string& error);

// "New folder", "New folder (2)", ... whichever does not exist yet.
std::string UniqueNewName(const Tab& tab, const std::string& base);

bool CreateFolderIn(Tab& tab, const std::string& name, std::string& error);
bool CreateFileIn(Tab& tab, const std::string& name, std::string& error);
bool RenameEntry(Tab& tab, int entryIndex, const std::string& newName, std::string& error);
bool DeleteSelected(Tab& tab, bool permanent, std::string& error);

// Cut/copy place the selection on the Windows clipboard as CF_HDROP.
bool CopySelection(AppState& state, Tab& tab, bool cut, std::string& error);
bool CanPaste(const AppState& state, const Tab& tab);
// Pastes whatever file list the clipboard holds, from any program.
bool Paste(AppState& state, Tab& tab, std::string& error);

enum class DropAction
{
    Move,
    Copy,
};

// Explorer's rule: same drive moves, another drive copies; Ctrl forces a
// copy and Shift forces a move.
DropAction DefaultDropAction(const std::vector<std::string>& sources, const std::string& destDir, bool ctrl, bool shift);

// True when dropping these sources on destDir would do something: not the
// folder they already sit in (for a move), not a folder into itself.
bool CanDropOn(const std::vector<std::string>& sources, const std::string& destDir);

// Copies or moves sources into destDir and refreshes every tab.
bool DropPaths(AppState& state, const std::vector<std::string>& sources, const std::string& destDir,
               DropAction action, std::string& error);

} // namespace app
