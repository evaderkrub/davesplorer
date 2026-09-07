// Drag and drop of files between the panes, tabs, breadcrumbs and other
// programs. Every place that shows a folder calls FileDropTarget after its
// item; every file row calls FileDragSource.
#pragma once

#include "app/AppState.h"
#include "ui/UiState.h"

#include "imgui.h"
#include "imgui_internal.h"   // ImRect

#include <string>
#include <vector>

namespace ui
{

// Call right after a file-list row. Starts an in-app drag carrying the
// selection (the row itself if it was not selected).
void FileDragSource(app::AppState& state, UiState& ui, app::Tab& tab, int entryIndex);

// Call right after any item standing for a folder. Highlights while a drag
// (in-app or from another program) hovers it and performs the drop on
// release. Returns true when hovered by a drag this frame.
bool FileDropTarget(app::AppState& state, UiState& ui, const std::string& folderPath);

// The same for an area rather than an item: the empty part of a file list.
bool FileDropTargetRect(app::AppState& state, UiState& ui, const std::string& folderPath, const ImRect& rect, ImGuiID id);

// Copies or moves sources into dest, choosing the action from the
// modifiers, and reports failures through the error dialog.
void PerformDrop(app::AppState& state, UiState& ui, const std::vector<std::string>& sources,
                 const std::string& dest, bool ctrl, bool shift);

// Once per frame, before any pane draws.
void BeginDragDropFrame(app::AppState& state, UiState& ui);

} // namespace ui
