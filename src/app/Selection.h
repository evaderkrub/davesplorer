// Selection rules for the file list: click, ctrl-click, shift-click and
// keyboard movement, all operating on visible positions.
#pragma once

#include "app/Tab.h"

#include <cstdint>
#include <string>
#include <vector>

namespace app
{

// visiblePos indexes tab.visible. Plain click selects only that row;
// ctrl toggles it; shift selects the range from the anchor.
void ClickSelect(Tab& tab, int visiblePos, bool ctrl, bool shift);

// Arrow keys and Home/End: moves focus by delta rows (or to an end when
// toEnd is set), extending the selection when shift is held.
void MoveFocus(Tab& tab, int delta, bool toEnd, bool shift, bool ctrl);

void SelectAll(Tab& tab);
void ClearSelection(Tab& tab);
void InvertSelection(Tab& tab);

int      SelectedCount(const Tab& tab);
uint64_t SelectedBytes(const Tab& tab);
std::vector<int>         SelectedIndices(const Tab& tab);
std::vector<std::string> SelectedPaths(const Tab& tab);

// Position of an entry within tab.visible, or -1 if filtered out.
int VisiblePosOf(const Tab& tab, int entryIndex);

} // namespace app
