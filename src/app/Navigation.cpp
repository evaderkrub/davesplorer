#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "platform/FileSystem.h"

namespace app
{

namespace
{

void SetLocation(Tab& tab, const std::string& path)
{
    tab.path = path;
    tab.filter.clear();
    tab.focused = -1;
    tab.anchor = -1;
    tab.needsReload = true;
}

} // namespace

bool NavigateTo(Tab& tab, const std::string& rawPath, std::string& error)
{
    const std::string path = NormalizePath(rawPath);
    if (!path.empty() && !platform::IsDirectory(path))
        {
        error = platform::PathExists(path)
                    ? "'" + path + "' is a file, not a folder."
                    : "Can't find '" + path + "'. Check the spelling and try again.";
        return false;
        }
    // A forward branch is discarded once the user goes somewhere new, as in
    // every browser.
    if (tab.historyIndex >= 0 && tab.historyIndex + 1 < (int)tab.history.size())
        tab.history.resize((size_t)tab.historyIndex + 1);
    // Navigating to where we already are is a refresh, not a history entry.
    if (tab.historyIndex < 0 || tab.history[(size_t)tab.historyIndex] != path)
        {
        tab.history.push_back(path);
        tab.historyIndex = (int)tab.history.size() - 1;
        }
    SetLocation(tab, path);
    return true;
}

bool CanGoBack(const Tab& tab) { return tab.historyIndex > 0; }
bool CanGoForward(const Tab& tab) { return tab.historyIndex >= 0 && tab.historyIndex + 1 < (int)tab.history.size(); }
bool CanGoUp(const Tab& tab) { return !tab.path.empty(); }

void GoBack(Tab& tab)
{
    if (!CanGoBack(tab)) return;
    --tab.historyIndex;
    SetLocation(tab, tab.history[(size_t)tab.historyIndex]);
}

void GoForward(Tab& tab)
{
    if (!CanGoForward(tab)) return;
    ++tab.historyIndex;
    SetLocation(tab, tab.history[(size_t)tab.historyIndex]);
}

void GoUp(Tab& tab)
{
    if (!CanGoUp(tab)) return;
    std::string ignored;
    NavigateTo(tab, ParentPath(tab.path), ignored);
}

void Refresh(Tab& tab)
{
    tab.needsReload = true;
}

} // namespace app
