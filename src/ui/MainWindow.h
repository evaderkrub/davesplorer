// One frame of interface: host window with menu bar and dockspace, the
// Navigation pane, one dockable window per open location, the dialogs, and
// the global shortcuts.
#pragma once

#include "app/AppState.h"
#include "ui/UiState.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>

namespace ui
{

// Names the tests and the docking layout refer to.
inline constexpr const char* kHostWindow = "Davesplorer";
inline constexpr const char* kNavWindow  = "Navigation";

// A view window is titled after its folder; the "###view<id>" suffix keeps
// its identity (dock position, tests) stable as the title changes.
std::string ViewWindowName(const app::AppState& state, const app::Tab& tab);
inline ImGuiID ViewWindowId(int tabId) { return ImHashStr(("###view" + std::to_string(tabId)).c_str()); }

void DrawFrame(app::AppState& state, UiState& ui);

// Opens a location in a new view, focused, docked beside the active view
// (or into dockNode when given). Returns the new tab's index.
int OpenView(app::AppState& state, UiState& ui, const std::string& path, ImGuiID dockNode = 0);

// Asks for a new view split off the given tab's dock node next frame.
void RequestSplitView(UiState& ui, int tabId, ImGuiDir dir);

// Requests used by more than one pane.
void RequestDialog(UiState& ui, Dialog which, const std::string& title = {});
void ShowError(UiState& ui, const std::string& message);
void ZoomBy(app::AppState& state, float delta);
void ZoomReset(app::AppState& state);

// Pane bodies, each drawn inside its own Begin/End by DrawFrame.
void DrawShortcutBar(app::AppState& state, UiState& ui);   // inside the host window, under the menu
void DrawNavPane(app::AppState& state, UiState& ui);
// One location's window. Returns true when the user closed it.
bool DrawView(app::AppState& state, UiState& ui, int tabIndex);
void DrawDialogs(app::AppState& state, UiState& ui);

} // namespace ui
