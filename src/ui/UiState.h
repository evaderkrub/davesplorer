// Transient interface state: edit buffers, which dialog is open, tree
// caches. Nothing here is application state; it can be thrown away and the
// app is unchanged.
#pragma once

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

struct UiState
{
    // Address bar
    bool        addressEditing = false;
    bool        addressWantFocus = false;
    std::string addressText;

    bool searchWantFocus = false;

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

    // File list housekeeping
    uint64_t seenGeneration = 0;   // last listing the table scrolled to top for
    int      scrollToEntry = -1;   // keyboard focus moved off-screen
    int      selectTabId = -1;     // programmatic tab switch (Ctrl+Tab)
    bool     filesWantFocus = false;

    float dpiScale = 1.0f;         // from the host each frame
    bool  resetLayoutRequested = false;
    bool  showMetrics = false;
    bool  showDemo = false;
    bool  showTestEngine = false;
};

} // namespace ui
