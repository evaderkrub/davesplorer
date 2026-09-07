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

void CopySelection(AppState& state, Tab& tab, bool cut);
bool CanPaste(const AppState& state, const Tab& tab);
bool Paste(AppState& state, Tab& tab, std::string& error);

} // namespace app
