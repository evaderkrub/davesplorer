#include "app/Selection.h"

#include <algorithm>

namespace app
{

namespace
{

void SelectRange(Tab& tab, int fromPos, int toPos)
{
    if (fromPos > toPos) std::swap(fromPos, toPos);
    for (int p = fromPos; p <= toPos; ++p)
        if (p >= 0 && p < (int)tab.visible.size())
            tab.selected[(size_t)tab.visible[(size_t)p]] = 1;
}

} // namespace

int VisiblePosOf(const Tab& tab, int entryIndex)
{
    for (size_t p = 0; p < tab.visible.size(); ++p)
        if (tab.visible[p] == entryIndex) return (int)p;
    return -1;
}

void ClickSelect(Tab& tab, int visiblePos, bool ctrl, bool shift)
{
    if (visiblePos < 0 || visiblePos >= (int)tab.visible.size()) return;
    const int entry = tab.visible[(size_t)visiblePos];
    if (tab.selected.size() != tab.entries.size()) tab.selected.assign(tab.entries.size(), 0);

    if (shift)
        {
        const int anchorPos = (tab.anchor >= 0) ? VisiblePosOf(tab, tab.anchor) : -1;
        if (!ctrl) ClearSelection(tab);
        SelectRange(tab, anchorPos >= 0 ? anchorPos : visiblePos, visiblePos);
        }
    else if (ctrl)
        {
        tab.selected[(size_t)entry] = tab.selected[(size_t)entry] ? 0 : 1;
        tab.anchor = entry;
        }
    else
        {
        ClearSelection(tab);
        tab.selected[(size_t)entry] = 1;
        tab.anchor = entry;
        }
    tab.focused = entry;
}

void MoveFocus(Tab& tab, int delta, bool toEnd, bool shift, bool ctrl)
{
    if (tab.visible.empty()) return;
    const int count = (int)tab.visible.size();
    int pos = (tab.focused >= 0) ? VisiblePosOf(tab, tab.focused) : -1;
    if (toEnd)               pos = delta < 0 ? 0 : count - 1;
    else if (pos < 0)        pos = delta < 0 ? count - 1 : 0;
    else                     pos = std::clamp(pos + delta, 0, count - 1);

    if (ctrl && !shift)
        {
        // Ctrl+arrow moves the focus ring without touching the selection,
        // so a later Ctrl+Space can add the row.
        tab.focused = tab.visible[(size_t)pos];
        return;
        }
    ClickSelect(tab, pos, false, shift);
}

void SelectAll(Tab& tab)
{
    if (tab.selected.size() != tab.entries.size()) tab.selected.assign(tab.entries.size(), 0);
    for (int i : tab.visible) tab.selected[(size_t)i] = 1;
}

void ClearSelection(Tab& tab)
{
    std::fill(tab.selected.begin(), tab.selected.end(), (uint8_t)0);
}

void InvertSelection(Tab& tab)
{
    if (tab.selected.size() != tab.entries.size()) tab.selected.assign(tab.entries.size(), 0);
    for (int i : tab.visible) tab.selected[(size_t)i] = tab.selected[(size_t)i] ? 0 : 1;
}

int SelectedCount(const Tab& tab)
{
    int n = 0;
    for (uint8_t s : tab.selected) n += s ? 1 : 0;
    return n;
}

uint64_t SelectedBytes(const Tab& tab)
{
    uint64_t total = 0;
    for (size_t i = 0; i < tab.selected.size() && i < tab.entries.size(); ++i)
        if (tab.selected[i] && !tab.entries[i].isDirectory) total += tab.entries[i].size;
    return total;
}

std::vector<int> SelectedIndices(const Tab& tab)
{
    std::vector<int> out;
    // Report in display order so operations happen in the order the user sees.
    for (int i : tab.visible)
        if (i < (int)tab.selected.size() && tab.selected[(size_t)i]) out.push_back(i);
    return out;
}

std::vector<std::string> SelectedPaths(const Tab& tab)
{
    std::vector<std::string> out;
    for (int i : SelectedIndices(tab)) out.push_back(tab.entries[(size_t)i].path);
    return out;
}

} // namespace app
