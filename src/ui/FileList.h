// What the two layouts of a folder's contents share: the details table
// (FilesPane.cpp) and the thumbnail grid (FileGrid.cpp) both show the
// same rows with the same selection, lasso, drag, context-menu and
// open-on-double-click behaviour. Internal to the ui library.
#pragma once

#include "app/AppState.h"
#include "ui/UiState.h"

#include "imgui.h"
#include "imgui_internal.h"   // ImRect

#include <string>

namespace ui
{

// Enter or double-click on one entry: a folder navigates, an image opens
// in the built-in viewer (when that is on), anything else goes to the
// program the shell associates with it. Never moves state.tabs.
bool OpenEntryHere(app::AppState& state, UiState& ui, int tabIndex, int entryIndex, std::string& error);

// The popups. Drawn inside BeginPopup("RowContext") / ("BackgroundContext").
void DrawRowContextMenu(app::AppState& state, UiState& ui, int tabIndex);
void DrawBackgroundContextMenu(app::AppState& state, UiState& ui, app::Tab& tab);

// A new listing arrived since this view last drew: scroll to the top, or
// to the focused entry when there is one, and cancel any lasso. Call
// inside the scrolling window.
void NoticeNewListing(app::Tab& tab, ViewUi& v);

// The rubber-band rectangle for this frame (only meaningful while pending
// or active), promoting a pending press to an active lasso once the mouse
// has moved.
ImRect UpdateLasso(app::Tab& tab, ViewUi& v);

// Selects or deselects one item by whether the lasso touches its rect. A
// details row reads as full-width, so it tests vertical overlap only; a
// grid cell tests the whole rectangle.
void ApplyLassoToItem(app::Tab& tab, ViewUi& v, int entryIndex, const ImRect& lasso, const ImRect& item, bool verticalOnly);

// Draws the band clipped to the current window, and ends the lasso on
// release (a press that never moved was a plain click: it clears).
void DrawLassoBand(const ViewUi& v, const ImRect& lasso);
void EndLassoOnRelease(app::Tab& tab, ViewUi& v);

// What the item loop noticed, resolved after the loop so a selection
// change cannot shift items mid-iteration.
struct ListClicks
{
    bool itemHovered = false;
    int  pendingClick = -1;      // visible position
    bool pendingDouble = false;
    int  pendingContext = -1;    // visible position
};
void ResolveListClicks(app::AppState& state, UiState& ui, int tabIndex, const ListClicks& clicks);
void DrawListPopups(app::AppState& state, UiState& ui, int tabIndex);

// The name as shown: without its extension when that option is off.
std::string ShownName(const app::Tab& tab, const platform::FileEntry& e, const app::Settings& settings);

// The grid layout.
void DrawThumbnailGrid(app::AppState& state, UiState& ui, int tabIndex);

} // namespace ui
