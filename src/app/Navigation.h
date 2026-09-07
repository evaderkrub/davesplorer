// Moving a tab between locations, with back/forward history.
#pragma once

#include "app/Tab.h"

#include <string>

namespace app
{

// Records the new location in history and marks the tab for reload. The
// path is normalized first. Returns false (with the reason) when the path
// is not a folder that exists; the tab is left where it was.
bool NavigateTo(Tab& tab, const std::string& path, std::string& error);

bool CanGoBack(const Tab& tab);
bool CanGoForward(const Tab& tab);
bool CanGoUp(const Tab& tab);

void GoBack(Tab& tab);
void GoForward(Tab& tab);
void GoUp(Tab& tab);
void Refresh(Tab& tab);

} // namespace app
