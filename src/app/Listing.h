// Filling a tab from the OS and deriving the visible, sorted view.
#pragma once

#include "app/Settings.h"
#include "app/Tab.h"

namespace app
{

// Re-reads the folder (or the drive list for This PC) if the tab asked for
// it. Selection survives a reload when the same names are still present.
void ReloadIfNeeded(Tab& tab, const Settings& settings);

// Rebuilds tab.visible from the search filter, hidden-file setting and sort
// order. Cheap enough to call whenever any of those change.
void ApplyView(Tab& tab, const Settings& settings);

// Folders first, then Explorer's natural order on the chosen column.
void SortEntries(Tab& tab);

} // namespace app
