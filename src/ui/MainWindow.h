// One frame of interface: host window with menu bar and dockspace, the
// Navigation and Files panes, the dialogs, and the global shortcuts.
#pragma once

#include "app/AppState.h"
#include "ui/UiState.h"

namespace ui
{

// Names the tests and the docking layout refer to.
inline constexpr const char* kHostWindow  = "Davesplorer";
inline constexpr const char* kNavWindow   = "Navigation";
inline constexpr const char* kFilesWindow = "Files";

void DrawFrame(app::AppState& state, UiState& ui);

// Requests used by more than one pane.
void RequestDialog(UiState& ui, Dialog which, const std::string& title = {});
void ShowError(UiState& ui, const std::string& message);
void ZoomBy(app::AppState& state, float delta);
void ZoomReset(app::AppState& state);

// Pane bodies, each drawn inside its own Begin/End by DrawFrame.
void DrawNavPane(app::AppState& state, UiState& ui);
void DrawFilesPane(app::AppState& state, UiState& ui);
void DrawDialogs(app::AppState& state, UiState& ui);

} // namespace ui
