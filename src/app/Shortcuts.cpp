#include "app/Shortcuts.h"
#include "app/Selection.h"

namespace app
{

namespace
{

bool ValidSlot(const AppState& state, int slot)
{
    return slot >= 0 && slot < kShortcutSlots && (size_t)slot < state.settings.shortcuts.size();
}

} // namespace

std::string ShortcutCandidate(const Tab& tab)
{
    const std::vector<int> selected = SelectedIndices(tab);
    if (selected.size() == 1 && tab.entries[(size_t)selected[0]].isDirectory)
        return tab.entries[(size_t)selected[0]].path;
    return tab.path;
}

bool ShortcutAssigned(const AppState& state, int slot)
{
    return ValidSlot(state, slot) && !state.settings.shortcuts[(size_t)slot].empty();
}

const std::string& ShortcutPath(const AppState& state, int slot)
{
    static const std::string none;
    return ValidSlot(state, slot) ? state.settings.shortcuts[(size_t)slot] : none;
}

void AssignShortcut(AppState& state, int slot, const std::string& path)
{
    if (!ValidSlot(state, slot) || path.empty()) return;
    state.settings.shortcuts[(size_t)slot] = path;
    SaveAppState(state);
}

void ClearShortcut(AppState& state, int slot)
{
    if (!ValidSlot(state, slot)) return;
    state.settings.shortcuts[(size_t)slot].clear();
    SaveAppState(state);
}

} // namespace app
