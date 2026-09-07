// The shortcut bar: numbered slots that each remember one folder.
#pragma once

#include "app/AppState.h"

#include <string>

namespace app
{

// What an empty slot would be assigned from this tab: the one selected
// folder if exactly one is selected, otherwise the folder being shown.
// Empty when the tab is on This PC with nothing selected.
std::string ShortcutCandidate(const Tab& tab);

bool ShortcutAssigned(const AppState& state, int slot);
const std::string& ShortcutPath(const AppState& state, int slot);

// Both write settings.ini at once, so a crash later cannot lose the change.
void AssignShortcut(AppState& state, int slot, const std::string& path);
void ClearShortcut(AppState& state, int slot);

} // namespace app
