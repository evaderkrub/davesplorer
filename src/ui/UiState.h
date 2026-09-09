// Transient interface state: edit buffers, which dialog is open, tree
// caches. Nothing here is application state; it can be thrown away and the
// app is unchanged.
#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui
{

enum class Dialog
{
    None,
    About,
    NewFolder,
    NewFile,
    Rename,
    Delete,
    Error,
};

// Per-view interface state, keyed by the tab's id. Every open location is
// its own dockable window, so each needs its own address bar mode, scroll
// requests and docking wishes.
struct ViewUi
{
    bool        addressEditing = false;
    bool        addressWantFocus = false;
    std::string addressText;
    bool        searchWantFocus = false;

    uint64_t seenGeneration = 0;   // last listing the table scrolled to top for
    int      scrollToEntry = -1;   // keyboard focus moved off-screen

    bool    shown = false;         // Begin() has run at least once
    bool    wantFocus = false;     // bring the window (and its dock tab) to front
    ImGuiID dockId = 0;            // where the window sat last frame, 0 when floating
    ImGuiID dockHint = 0;          // dock here on the next Begin (a split just made the node)

    // Rubber-band (lasso) selection: press in empty space and drag a
    // rectangle over rows. Pending until the mouse moves, so a plain click
    // still just clears the selection.
    bool                 lassoPending = false;
    bool                 lassoActive = false;
    bool                 lassoAdditive = false;   // Ctrl/Shift held: add to the existing selection
    ImVec2               lassoAnchor{};           // screen-space press point
    std::vector<uint8_t> lassoBase;               // selection snapshot when the lasso began
};

// Per-file-view interface state: the decoded image as a texture, and how
// it is being looked at. Keyed by the file view's id.
struct FileViewUi
{
    bool    shown = false;
    bool    wantFocus = false;
    ImGuiID dockId = 0;
    ImGuiID dockHint = 0;

    std::string    loadedPath;       // what `texture` holds; differs from the view's path until loaded
    ImTextureData* texture = nullptr;
    int            width = 0;        // texture size (may be downsampled from the file)
    int            height = 0;
    int            sourceWidth = 0;  // the file's own size
    int            sourceHeight = 0;
    bool           hasAlpha = false; // draw a checkerboard behind it
    uint64_t       fileBytes = 0;
    std::string    error;            // decode failure, shown in place of the image

    bool   fit = true;               // follow the window size (shrink only); false once zoomed by hand
    float  zoom = 1.0f;              // texture pixels to screen pixels
    ImVec2 pan{};                    // image centre offset from the canvas centre, screen pixels

    std::vector<std::string> siblings;   // the folder's images in order, for Previous/Next
    int                      siblingIndex = -1;
};

struct UiState
{
    std::unordered_map<int, ViewUi> views;
    ViewUi& view(int tabId) { return views[tabId]; }

    std::unordered_map<int, FileViewUi> fileViews;
    FileViewUi& fileView(int id) { return fileViews[id]; }
    int activeFileView = -1;       // the focused file view's id, -1 when a folder view has focus

    // View > Split: open a new view beside the given one.
    struct SplitRequest
        {
        int      tabId = -1;
        ImGuiDir dir = ImGuiDir_Right;
        } splitRequest;
    ImGuiID defaultViewDock = 0;   // the dockspace's central node, where new views land

    // Dialogs. `requested` is consumed by the dialog drawer, which lives in
    // the host window so OpenPopup and BeginPopupModal share an ID stack.
    Dialog      dialog = Dialog::None;
    bool        dialogRequested = false;
    std::string dialogText;      // name being typed
    std::string dialogMessage;   // error text, delete summary
    std::string dialogTitle;
    int         dialogEntry = -1;
    bool        deletePermanent = false;

    // Navigation tree: subfolders per path, filled on first expand.
    std::unordered_map<std::string, std::vector<std::string>> treeChildren;

    // Drag and drop inside the app: the paths being dragged and the folder
    // under the cursor (last frame's answer, used by the drag tooltip).
    std::vector<std::string> dragPaths;
    std::string dropHoverPath;
    std::string dropHoverPathNext;
    bool        rowDropHovered = false;      // a row took the drop this frame; the background must not
    bool        externalDragRequested = false; // the drag left the window: the host hands it to OLE

    // A drag from another program, as SDL reports it.
    struct ExternalDrop
        {
        bool active = false;
        bool completed = false;
        bool ctrl = false, shift = false;
        std::vector<std::string> paths;
        std::string targetPath;              // folder under the cursor, if any
        bool haveTarget = false;
        } externalDrop;

    float dpiScale = 1.0f;         // from the host each frame
    bool  resetLayoutRequested = false;
    bool  showMetrics = false;
    bool  showDemo = false;
};

} // namespace ui
