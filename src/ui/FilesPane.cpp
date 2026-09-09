// A view: one open location as a dockable window with toolbar, address bar,
// search, file table and status bar.
#include "ui/MainWindow.h"
#include "ui/DragDrop.h"
#include "ui/FileList.h"
#include "ui/Fonts.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/FileOps.h"
#include "app/Format.h"
#include "app/Image.h"
#include "app/Listing.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "platform/FileSystem.h"
#include "platform/Shell.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ui
{

std::string ViewWindowName(const app::AppState& state, const app::Tab& tab)
{
    return app::LocationTitle(state, tab.path) + "###view" + std::to_string(tab.id);
}

namespace
{

void Navigate(UiState& ui, app::Tab& tab, const std::string& path)
{
    std::string error;
    if (!app::NavigateTo(tab, path, error)) ShowError(ui, error);
}

} // namespace

bool OpenEntryHere(app::AppState& state, UiState& ui, int tabIndex, int entryIndex, std::string& error)
{
    const app::Tab& tab = state.tabs[(size_t)tabIndex];
    if (entryIndex < 0 || entryIndex >= (int)tab.entries.size()) return false;
    const platform::FileEntry& e = tab.entries[(size_t)entryIndex];
    if (!e.isDirectory && state.settings.openImagesInApp && app::IsViewableImage(e.extension))
        {
        OpenFileView(state, ui, e.path);
        return true;
        }
    return app::OpenEntry(state, state.tabs[(size_t)tabIndex], entryIndex, error);
}

namespace
{

// Enter or double-click on the selection. Takes the tab by index because
// opening a view reallocates state.tabs.
void OpenSelectedOrFocused(app::AppState& state, UiState& ui, int tabIndex)
{
    std::vector<int> targets;
    std::vector<std::string> extraFolders;
    {
    const app::Tab& tab = state.tabs[(size_t)tabIndex];
    targets = app::SelectedIndices(tab);
    if (targets.empty() && tab.focused >= 0) targets.push_back(tab.focused);
    // Enter on several folders: the first opens here, the others each get
    // a view, the way Explorer would give each its own window.
    bool firstFolder = true;
    std::vector<int> kept;
    for (int i : targets)
        {
        const platform::FileEntry& e = tab.entries[(size_t)i];
        if (e.isDirectory && !firstFolder) extraFolders.push_back(e.path);
        else kept.push_back(i);
        if (e.isDirectory) firstFolder = false;
        }
    targets = kept;
    }
    std::string error;
    for (int i : targets)
        if (!OpenEntryHere(state, ui, tabIndex, i, error)) ShowError(ui, error);
    for (const std::string& folder : extraFolders) OpenView(state, ui, folder);
}

void DrawAddressBar(app::AppState& state, UiState& ui, app::Tab& tab, ViewUi& v, float width)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight();

    if (v.addressEditing)
        {
        ImGui::SetNextItemWidth(width);
        if (v.addressWantFocus)
            {
            ImGui::SetKeyboardFocusHere();
            v.addressWantFocus = false;
            }
        const bool entered = ImGui::InputText("##address", &v.addressText,
                                              ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool active = ImGui::IsItemActive();
        if (entered)
            {
            v.addressEditing = false;
            std::string error;
            if (!app::NavigateTo(tab, v.addressText, error)) ShowError(ui, error);
            }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (!active && !ImGui::IsItemActivated() && ImGui::IsItemDeactivated()))
            {
            v.addressEditing = false;
            }
        else if (!active && !ImGui::IsItemFocused() && !ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
            // Clicked somewhere else: leave edit mode, keep the old path.
            v.addressEditing = false;
            }
        return;
        }

    // Breadcrumbs inside a frame-styled child so it reads as one control.
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x * 0.5f, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
    ImGui::BeginChild("##addressbar", ImVec2(width, height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x * 0.6f, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().textMuted);
    const std::vector<app::Crumb> crumbs = app::Breadcrumbs(tab.path);
    std::vector<std::string> labels;
    for (size_t i = 0; i < crumbs.size(); ++i)
        labels.push_back((i == 1 && app::IsDriveRoot(crumbs[i].path)) ? app::LocationTitle(state, crumbs[i].path)
                                                                       : crumbs[i].label);

    // Deep paths do not fit: like Explorer, the leading crumbs collapse into
    // a "..." button that lists them. The last crumb is always shown, and a
    // click-to-edit area is always kept free at the end.
    const float padX = ImGui::GetStyle().FramePadding.x * 2.0f;
    const float chevronW = ImGui::CalcTextSize(ICON_MD_CHEVRON_RIGHT).x;
    const float editArea = height * 1.5f;
    float total = 0.0f;
    std::vector<float> cost(crumbs.size());
    for (size_t i = 0; i < crumbs.size(); ++i)
        {
        cost[i] = ImGui::CalcTextSize(labels[i].c_str()).x + padX + chevronW;
        total += cost[i];
        }
    size_t first = 0;
    if (total > width - editArea)
        {
        const float avail = width - editArea - (ImGui::CalcTextSize(ICON_MD_MORE_HORIZ).x + padX + chevronW);
        while (first + 1 < crumbs.size() && total > avail)
            total -= cost[first++];
        }

    std::string clicked;
    bool haveClick = false;
    if (first > 0)
        {
        if (ImGui::Button(ICON_MD_MORE_HORIZ "###crumbmore", ImVec2(0, height))) ImGui::OpenPopup("crumbmore");
        ImGui::SameLine();
        if (ImGui::BeginPopup("crumbmore"))
            {
            for (size_t i = 0; i < first; ++i)
                if (ImGui::MenuItem((labels[i] + "###crumbmenu" + std::to_string(i)).c_str()))
                    {
                    clicked = crumbs[i].path;
                    haveClick = true;
                    }
            ImGui::EndPopup();
            }
        }
    for (size_t i = first; i < crumbs.size(); ++i)
        {
        if (i > 0)
            {
            ImGui::TextUnformatted(ICON_MD_CHEVRON_RIGHT);
            ImGui::SameLine();
            }
        const bool last = i + 1 == crumbs.size();
        if (last) ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().text);
        const std::string id = labels[i] + "###crumb" + std::to_string(i);
        if (ImGui::Button(id.c_str(), ImVec2(0, height)))
            {
            clicked = crumbs[i].path;
            haveClick = true;
            }
        if (last) ImGui::PopStyleColor();
        if (!crumbs[i].path.empty()) FileDropTarget(state, ui, crumbs[i].path);
        ImGui::SameLine();
        }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // The rest of the bar: clicking it turns the crumbs into a text field.
    const float remaining = std::max(ImGui::GetContentRegionAvail().x, editArea * 0.5f);
    if (ImGui::InvisibleButton("###addressclick", ImVec2(remaining, height)))
        {
        v.addressEditing = true;
        v.addressWantFocus = true;
        v.addressText = tab.path;
        }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    ImGui::EndChild();
    if (haveClick) Navigate(ui, tab, clicked);
}

// Returns false when the toolbar opened a view (state.tabs moved).
bool DrawToolbar(app::AppState& state, UiState& ui, int tabIndex)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);
    if (IconButton("Back", ICON_MD_ARROW_BACK, "Back (Alt+Left)", app::CanGoBack(tab))) app::GoBack(tab);
    ImGui::SameLine();
    if (IconButton("Forward", ICON_MD_ARROW_FORWARD, "Forward (Alt+Right)", app::CanGoForward(tab))) app::GoForward(tab);
    ImGui::SameLine();
    if (IconButton("Up", ICON_MD_ARROW_UPWARD, "Up (Alt+Up)", app::CanGoUp(tab))) app::GoUp(tab);
    ImGui::SameLine();
    if (IconButton("Refresh", ICON_MD_REFRESH, "Refresh (F5)"))
        {
        app::Refresh(tab);
        ui.treeChildren.clear();
        }
    ImGui::SameLine();

    // Right-hand cluster: view mode, new view, split right, split down.
    // Measured from the real icon widths so a narrow view does not push
    // them off the edge.
    const ImGuiStyle& style = ImGui::GetStyle();
    auto buttonW = [&](const char* icon) { return ImGui::CalcTextSize(icon).x + style.FramePadding.x * 2.0f; };
    const float cluster = buttonW(ICON_MD_GRID_VIEW) + buttonW(ICON_MD_ADD) + buttonW(ICON_MD_VERTICAL_SPLIT) +
                          buttonW(ICON_MD_HORIZONTAL_SPLIT) + style.ItemSpacing.x * 4.0f;
    const float avail = ImGui::GetContentRegionAvail().x - cluster - style.ItemSpacing.x;
    const float searchWidth = std::clamp(avail * 0.25f, 90.0f, 320.0f);
    const float addressWidth = std::max(avail - searchWidth, 60.0f);
    DrawAddressBar(state, ui, tab, v, addressWidth);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(searchWidth);
    if (v.searchWantFocus)
        {
        ImGui::SetKeyboardFocusHere();
        v.searchWantFocus = false;
        }
    const std::string hint = std::string(ICON_MD_SEARCH) + " Search " + app::LocationTitle(state, tab.path);
    if (ImGui::InputTextWithHint("##search", hint.c_str(), &tab.filter)) app::ApplyView(tab, state.settings);
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape) && !tab.filter.empty())
        {
        tab.filter.clear();
        app::ApplyView(tab, state.settings);
        }
    ImGui::SameLine();

    // The toggle sets the default for new views too, as Explorer's view
    // choice sticks.
    const bool thumbs = tab.viewMode == app::ViewMode::Thumbnails;
    if (IconButton("ViewMode", thumbs ? ICON_MD_VIEW_LIST : ICON_MD_GRID_VIEW, thumbs ? "Details view" : "Thumbnails view"))
        {
        tab.viewMode = thumbs ? app::ViewMode::Details : app::ViewMode::Thumbnails;
        state.settings.viewMode = tab.viewMode;
        }
    ImGui::SameLine();

    const std::string path = tab.path;
    const int id = tab.id;
    if (IconButton("NewTab", ICON_MD_ADD, "Open this folder in a new view (Ctrl+T)"))
        {
        OpenView(state, ui, path);
        return false;
        }
    ImGui::SameLine();
    if (IconButton("SplitRight", ICON_MD_VERTICAL_SPLIT, "Split: new view to the right")) RequestSplitView(ui, id, ImGuiDir_Right);
    ImGui::SameLine();
    if (IconButton("SplitDown", ICON_MD_HORIZONTAL_SPLIT, "Split: new view below")) RequestSplitView(ui, id, ImGuiDir_Down);
    return true;
}

} // namespace

void DrawRowContextMenu(app::AppState& state, UiState& ui, int tabIndex)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    const bool inFolder = !tab.path.empty();
    const bool haveSelection = app::SelectedCount(tab) > 0;
    const bool focusedIsFolder = tab.focused >= 0 && tab.entries[(size_t)tab.focused].isDirectory;
    const bool focusedIsImage = tab.focused >= 0 && !focusedIsFolder && app::IsViewableImage(tab.entries[(size_t)tab.focused].extension);
    const std::string focusedPath = tab.focused >= 0 ? tab.entries[(size_t)tab.focused].path : std::string();
    if (ImGui::MenuItem("Open"))
        {
        OpenSelectedOrFocused(state, ui, tabIndex);
        return;   // views may have changed; `tab` is not to be trusted past here
        }
    if (ImGui::MenuItem("Open in new view", nullptr, false, focusedIsFolder))
        {
        OpenView(state, ui, focusedPath);
        return;
        }
    // The two ways to open an image, both always on offer whatever the
    // double-click preference says.
    if (ImGui::MenuItem("View in Davesplorer", nullptr, false, focusedIsImage)) OpenFileView(state, ui, focusedPath);
    if (ImGui::MenuItem("Open with default program", nullptr, false, tab.focused >= 0 && !focusedIsFolder))
        {
        std::string error;
        for (const std::string& p : app::SelectedPaths(tab))
            if (!platform::IsDirectory(p) && !platform::OpenWithShell(p, error)) ShowError(ui, error);
        }
    if (ImGui::MenuItem("Show in system file manager"))
        {
        std::string error;
        const std::vector<int> sel = app::SelectedIndices(tab);
        if (!sel.empty() && !platform::ShowInExplorer(tab.entries[(size_t)sel[0]].path, error)) ShowError(ui, error);
        }
    ImGui::Separator();
    std::string clipError;
    if (ImGui::MenuItem("Cut", "Ctrl+X", false, haveSelection && inFolder) && !app::CopySelection(state, tab, true, clipError)) ShowError(ui, clipError);
    if (ImGui::MenuItem("Copy", "Ctrl+C", false, haveSelection && inFolder) && !app::CopySelection(state, tab, false, clipError)) ShowError(ui, clipError);
    if (ImGui::MenuItem("Copy as path"))
        {
        std::string text;
        for (const std::string& p : app::SelectedPaths(tab)) text += (text.empty() ? "" : "\n") + p;
        ImGui::SetClipboardText(text.c_str());
        }
    ImGui::Separator();
    if (ImGui::MenuItem("Rename", "F2", false, haveSelection && inFolder))
        {
        const std::vector<int> sel = app::SelectedIndices(tab);
        ui.dialogEntry = sel[0];
        ui.dialogText = tab.entries[(size_t)sel[0]].name;
        RequestDialog(ui, Dialog::Rename);
        }
    if (ImGui::MenuItem("Delete", "Del", false, haveSelection && inFolder))
        {
        ui.deletePermanent = false;
        ui.dialogMessage = "Are you sure you want to move the selected items to the Recycle Bin?";
        RequestDialog(ui, Dialog::Delete);
        }
}

void DrawBackgroundContextMenu(app::AppState& state, UiState& ui, app::Tab& tab)
{
    const bool inFolder = !tab.path.empty();
    if (ImGui::MenuItem("New folder", "Ctrl+Shift+N", false, inFolder))
        {
        ui.dialogText = app::UniqueNewName(tab, "New folder");
        RequestDialog(ui, Dialog::NewFolder);
        }
    if (ImGui::MenuItem("New text file", nullptr, false, inFolder))
        {
        ui.dialogText = app::UniqueNewName(tab, "New Text Document.txt");
        RequestDialog(ui, Dialog::NewFile);
        }
    ImGui::Separator();
    if (ImGui::MenuItem("Paste", "Ctrl+V", false, app::CanPaste(state, tab)))
        {
        std::string error;
        if (!app::Paste(state, tab, error)) ShowError(ui, error);
        }
    if (ImGui::MenuItem("Select all", "Ctrl+A")) app::SelectAll(tab);
    ImGui::Separator();
    if (ImGui::MenuItem("Refresh", "F5")) app::Refresh(tab);
    if (ImGui::MenuItem("Open terminal here", nullptr, false, inFolder))
        {
        std::string error;
        if (!platform::OpenTerminalAt(tab.path, error)) ShowError(ui, error);
        }
}

void NoticeNewListing(app::Tab& tab, ViewUi& v)
{
    if (v.seenGeneration == tab.listingGeneration) return;
    v.seenGeneration = tab.listingGeneration;
    // A new listing normally starts at the top, but if it arrived with a
    // focused item -- a command-line /select, or a file kept across a
    // refresh -- bring that into view instead, the way Explorer does.
    if (tab.focused >= 0) v.scrollToEntry = tab.focused;
    else ImGui::SetScrollY(0.0f);
    // A reload replaces the items the lasso was drawn over; cancel it.
    v.lassoPending = v.lassoActive = false;
}

ImRect UpdateLasso(app::Tab& tab, ViewUi& v)
{
    ImRect rect;
    if (!v.lassoPending && !v.lassoActive) return rect;
    // Grow the rectangle from its anchor to the mouse, and promote a
    // pending press to an active lasso once the mouse has moved. A plain
    // lasso replaces the selection; Ctrl/Shift adds to it.
    const ImVec2 m = ImGui::GetIO().MousePos;
    rect = ImRect(ImVec2(std::min(v.lassoAnchor.x, m.x), std::min(v.lassoAnchor.y, m.y)),
                  ImVec2(std::max(v.lassoAnchor.x, m.x), std::max(v.lassoAnchor.y, m.y)));
    if (v.lassoPending && (std::fabs(m.x - v.lassoAnchor.x) + std::fabs(m.y - v.lassoAnchor.y)) > 4.0f)
        {
        v.lassoPending = false;
        v.lassoActive = true;
        if (!v.lassoAdditive) app::ClearSelection(tab);
        }
    return rect;
}

void ApplyLassoToItem(app::Tab& tab, ViewUi& v, int entryIndex, const ImRect& lasso, const ImRect& item, bool verticalOnly)
{
    if (!v.lassoActive) return;
    // Applied here, one frame before the highlight, which reads smoothly.
    // In additive mode the pre-lasso selection is kept too.
    const bool inLasso = verticalOnly ? (lasso.Min.y <= item.Max.y && lasso.Max.y >= item.Min.y)
                                      : lasso.Overlaps(item);
    const bool base = entryIndex < (int)v.lassoBase.size() && v.lassoBase[(size_t)entryIndex];
    tab.selected[(size_t)entryIndex] = (v.lassoAdditive ? (base || inLasso) : inLasso) ? 1 : 0;
}

void DrawLassoBand(const ViewUi& v, const ImRect& lasso)
{
    if (!v.lassoActive) return;
    // Clamped to the scrolling body so it does not paint over a frozen
    // header.
    ImRect band = lasso;
    band.ClipWith(ImGui::GetCurrentWindow()->ClipRect);
    if (band.Min.x >= band.Max.x || band.Min.y >= band.Max.y) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(band.Min, band.Max, WithAlpha(CurrentPalette().accent, 40));
    dl->AddRect(band.Min, band.Max, WithAlpha(CurrentPalette().accent, 180));
}

void EndLassoOnRelease(app::Tab& tab, ViewUi& v)
{
    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left) || !(v.lassoActive || v.lassoPending)) return;
    // A pending lasso that never moved was a plain background click, so
    // it clears the selection; an active one keeps what it gathered.
    if (v.lassoPending && !v.lassoActive) app::ClearSelection(tab);
    v.lassoPending = v.lassoActive = false;
}

void ResolveListClicks(app::AppState& state, UiState& ui, int tabIndex, const ListClicks& clicks)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);
    ImGuiIO& io = ImGui::GetIO();
    if (clicks.pendingContext >= 0)
        {
        const int entryIndex = tab.visible[(size_t)clicks.pendingContext];
        if (!(entryIndex < (int)tab.selected.size() && tab.selected[(size_t)entryIndex]))
            app::ClickSelect(tab, clicks.pendingContext, false, false);
        tab.focused = entryIndex;
        ImGui::OpenPopup("RowContext");
        }
    else if (clicks.pendingClick >= 0)
        {
        if (clicks.pendingDouble)
            {
            app::ClickSelect(tab, clicks.pendingClick, false, false);
            std::string error;
            if (!OpenEntryHere(state, ui, tabIndex, tab.visible[(size_t)clicks.pendingClick], error)) ShowError(ui, error);
            }
        else
            {
            app::ClickSelect(tab, clicks.pendingClick, io.KeyCtrl, io.KeyShift);
            }
        }
    else if (ImGui::IsWindowHovered() && !clicks.itemHovered && !ImGui::IsAnyItemHovered() && !ImGui::IsDragDropActive())
        {
        // Press in empty space begins a rubber-band; it stays pending (and
        // the selection untouched) until the mouse moves, so a plain click
        // still just clears the selection on release.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
            v.lassoPending = true;
            v.lassoActive = false;
            v.lassoAnchor = io.MousePos;
            v.lassoAdditive = io.KeyCtrl || io.KeyShift;
            v.lassoBase = tab.selected;
            }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("BackgroundContext");
        }
}

void DrawListPopups(app::AppState& state, UiState& ui, int tabIndex)
{
    if (ImGui::BeginPopup("RowContext"))
        {
        DrawRowContextMenu(state, ui, tabIndex);
        ImGui::EndPopup();
        }
    if (ImGui::BeginPopup("BackgroundContext"))
        {
        DrawBackgroundContextMenu(state, ui, state.tabs[(size_t)tabIndex]);
        ImGui::EndPopup();
        }
}

std::string ShownName(const app::Tab& tab, const platform::FileEntry& e, const app::Settings& settings)
{
    if (!settings.showExtensions && !e.isDirectory && !e.extension.empty() && !tab.path.empty())
        return e.name.substr(0, e.name.size() - e.extension.size() - 1);
    return e.name;
}

namespace
{

// Keys that act on the list. Only while this view (or a child of it) has
// focus, nobody is typing, and no popup is up.
void HandleListKeys(app::AppState& state, UiState& ui, int tabIndex)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);
    const bool shift = io.KeyShift, ctrl = io.KeyCtrl;
    const int before = tab.focused;

    // In the grid a row is `cols` items, so Up/Down step by that and
    // Left/Right by one; the table has one item per row.
    const bool grid = tab.viewMode == app::ViewMode::Thumbnails;
    const int cols = grid ? std::max(1, v.gridColumns) : 1;
    const int page = grid ? cols * std::max(1, v.gridRowsVisible) : 15;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  app::MoveFocus(tab, cols, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    app::MoveFocus(tab, -cols, false, shift, ctrl);
    if (grid && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) app::MoveFocus(tab, 1, false, shift, ctrl);
    if (grid && ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  app::MoveFocus(tab, -1, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))   app::MoveFocus(tab, page, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))     app::MoveFocus(tab, -page, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_Home))       app::MoveFocus(tab, -1, true, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_End))        app::MoveFocus(tab, 1, true, shift, ctrl);
    if (tab.focused != before) v.scrollToEntry = tab.focused;

    if (ImGui::IsKeyPressed(ImGuiKey_Space) && ctrl && tab.focused >= 0)
        tab.selected[(size_t)tab.focused] = tab.selected[(size_t)tab.focused] ? 0 : 1;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        {
        OpenSelectedOrFocused(state, ui, tabIndex);
        return;   // may have opened views and moved state.tabs
        }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) app::GoBack(tab);
    if (ImGui::IsKeyPressed(ImGuiKey_F2) && app::SelectedCount(tab) == 1 && !tab.path.empty())
        {
        ui.dialogEntry = app::SelectedIndices(tab)[0];
        ui.dialogText = tab.entries[(size_t)ui.dialogEntry].name;
        RequestDialog(ui, Dialog::Rename);
        }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && app::SelectedCount(tab) > 0 && !tab.path.empty())
        {
        const int n = app::SelectedCount(tab);
        ui.deletePermanent = shift;
        ui.dialogMessage = shift
            ? "Are you sure you want to permanently delete " + (n == 1 ? "this item" : "these " + std::to_string(n) + " items") + "?"
            : "Are you sure you want to move " + (n == 1 ? "this item" : "these " + std::to_string(n) + " items") + " to the Recycle Bin?";
        RequestDialog(ui, Dialog::Delete);
        }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) app::SelectAll(tab);
    std::string clipError;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && !tab.path.empty() && !app::CopySelection(state, tab, false, clipError)) ShowError(ui, clipError);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X, false) && !tab.path.empty() && !app::CopySelection(state, tab, true, clipError)) ShowError(ui, clipError);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && app::CanPaste(state, tab))
        {
        std::string error;
        if (!app::Paste(state, tab, error)) ShowError(ui, error);
        }
}

void DrawFileTable(app::AppState& state, UiState& ui, int tabIndex)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);
    const app::Settings& settings = state.settings;
    const float scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;

    // Fixed column widths with horizontal scrolling, as Explorer does: a
    // narrow view scrolls sideways instead of crushing the Name column.
    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                                  ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingFixedFit;
    // Leave a small empty strip on the right of the list. It is always
    // present, whatever the column widths, so a rubber-band always has an
    // empty place to start from -- even in a folder that fills the height.
    const float lassoMargin = 24.0f * scale;
    const ImVec2 tableOuter(std::max(ImGui::GetContentRegionAvail().x - lassoMargin, 160.0f), 0.0f);
    if (!ImGui::BeginTable("FileTable", 4, flags, tableOuter)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide |
                                    ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending, 300.0f * scale, 0);
    ImGui::TableSetupColumn("Date modified", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 150.0f * scale, 1);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f * scale, 2);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 90.0f * scale, 3);
    ImGui::TableHeadersRow();

    // The table's sort state is the source of truth; the tab follows it.
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
        {
        if (specs->SpecsCount > 0)
            {
            const app::SortColumn column = (app::SortColumn)specs->Specs[0].ColumnUserID;
            const bool ascending = specs->Specs[0].SortDirection != ImGuiSortDirection_Descending;
            if (column != tab.sort.column || ascending != tab.sort.ascending)
                {
                tab.sort.column = column;
                tab.sort.ascending = ascending;
                app::ApplyView(tab, settings);
                }
            }
        specs->SpecsDirty = false;
        }

    NoticeNewListing(tab, v);
    const ImRect lassoRect = UpdateLasso(tab, v);

    const ImVec4 selectedBg = ImGui::ColorConvertU32ToFloat4(WithAlpha(CurrentPalette().accent, 70));
    const ImVec4 hoveredBg  = ImGui::ColorConvertU32ToFloat4(WithAlpha(CurrentPalette().accent, 40));
    ImGui::PushStyleColor(ImGuiCol_Header, selectedBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoveredBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, selectedBg);

    ListClicks clicks;

    ImGuiListClipper clipper;
    clipper.Begin((int)tab.visible.size());
    if (v.scrollToEntry >= 0)
        {
        const int pos = app::VisiblePosOf(tab, v.scrollToEntry);
        if (pos >= 0) clipper.IncludeItemByIndex(pos);
        }
    while (clipper.Step())
        {
        for (int pos = clipper.DisplayStart; pos < clipper.DisplayEnd; ++pos)
            {
            const int entryIndex = tab.visible[(size_t)pos];
            const platform::FileEntry& e = tab.entries[(size_t)entryIndex];
            const bool isSelected = entryIndex < (int)tab.selected.size() && tab.selected[(size_t)entryIndex];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const std::string shownName = ShownName(tab, e, settings);

            if (v.scrollToEntry == entryIndex)
                {
                ImGui::SetScrollHereY(0.5f);
                v.scrollToEntry = -1;
                }

            // The selectable carries only the ID ("###name" shows nothing),
            // so the icon can be drawn in its own tint and the name in text
            // color on top of it. The ID is the file name, which is what the
            // tests refer to.
            const std::string label = "###" + e.name;
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Selectable(label.c_str(), isSelected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap);
            const bool rowHoveredNow = ImGui::IsItemHovered();
            // Rubber-band membership: the selectable spans all columns, so
            // its rect is the whole row, and rows read as full-width, so
            // the band selects by vertical overlap: a drag anywhere down
            // the list (the right margin included) grabs the rows its
            // height covers.
            ApplyLassoToItem(tab, v, entryIndex, lassoRect, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), true);
            if (!tab.path.empty()) FileDragSource(state, ui, tab, entryIndex);
            if (e.isDirectory && FileDropTarget(state, ui, e.path)) ui.rowDropHovered = true;

            // Cut items are ghosted until they are pasted, as in Explorer.
            const bool isCut = state.clipboard.cut && std::find(state.clipboard.paths.begin(), state.clipboard.paths.end(), e.path) != state.clipboard.paths.end();
            const char* icon = IconForEntry(e);
            const float iconW = ImGui::CalcTextSize(icon).x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(rowStart, isCut ? WithAlpha(IconTintForEntry(e), 110) : IconTintForEntry(e), icon);
            dl->AddText(ImVec2(rowStart.x + iconW + ImGui::GetStyle().ItemInnerSpacing.x, rowStart.y),
                        (e.isHidden || isCut) ? CurrentPalette().textFaint : CurrentPalette().text, shownName.c_str());

            if (rowHoveredNow) clicks.itemHovered = true;
            if (clicked)
                {
                clicks.pendingClick = pos;
                clicks.pendingDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) clicks.pendingContext = pos;
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNormal) && e.isReparsePoint)
                ImGui::SetTooltip("Link: %s", e.path.c_str());

            ImGui::TableNextColumn();
            if (e.modified != 0) MutedText(platform::FormatDateTime(e.modified).c_str());

            ImGui::TableNextColumn();
            MutedText(tab.typeNames.size() > (size_t)entryIndex ? tab.typeNames[(size_t)entryIndex].c_str() : "");

            ImGui::TableNextColumn();
            if (!e.isDirectory || tab.path.empty())
                {
                const std::string size = tab.path.empty() ? app::HumanSize(e.size) : app::SizeInKB(e.size);
                const float w = ImGui::CalcTextSize(size.c_str()).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - w));
                MutedText(size.c_str());
                }
            }
        }
    ImGui::PopStyleColor(3);

    DrawLassoBand(v, lassoRect);
    EndLassoOnRelease(tab, v);
    ResolveListClicks(state, ui, tabIndex, clicks);
    DrawListPopups(state, ui, tabIndex);
    ImGui::EndTable();
}

void DrawStatusBar(app::AppState& state, UiState& ui, app::Tab& tab)
{
    const int selected = app::SelectedCount(tab);
    std::string text = std::to_string(tab.visible.size()) + (tab.visible.size() == 1 ? " item" : " items");
    if (!tab.filter.empty()) text += " (of " + std::to_string(tab.entries.size()) + ")";
    if (selected > 0)
        {
        text += "    " + std::to_string(selected) + (selected == 1 ? " item selected" : " items selected");
        const uint64_t bytes = app::SelectedBytes(tab);
        if (bytes > 0) text += "  " + app::HumanSize(bytes);
        }
    if (!state.clipboard.paths.empty())
        text += "    " + std::to_string(state.clipboard.paths.size()) + (state.clipboard.cut ? " cut" : " copied");
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    MutedText(text.c_str());
    if (!state.statusMessage.empty())
        {
        ImGui::SameLine();
        FaintText(state.statusMessage.c_str());
        }

    // Right end: new folder, a terminal here, and the real Explorer on the
    // same folder. The first two need a real folder, so they are greyed
    // out on This PC.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonsW = ImGui::CalcTextSize(ICON_MD_CREATE_NEW_FOLDER).x + ImGui::CalcTextSize(ICON_MD_TERMINAL).x +
                           ImGui::CalcTextSize(ICON_MD_OPEN_IN_NEW).x + style.FramePadding.x * 6.0f + style.ItemSpacing.x * 2.0f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttonsW));
    const bool inFolder = !tab.path.empty();
    if (IconButton("NewFolder", ICON_MD_CREATE_NEW_FOLDER, "New folder (Ctrl+Shift+N)", inFolder))
        {
        ui.dialogText = app::UniqueNewName(tab, "New folder");
        RequestDialog(ui, Dialog::NewFolder);
        }
    ImGui::SameLine();
    if (IconButton("Terminal", ICON_MD_TERMINAL, "Open terminal here", inFolder))
        {
        std::string error;
        if (!platform::OpenTerminalAt(tab.path, error)) ShowError(ui, error);
        }
    ImGui::SameLine();
    if (IconButton("Explorer", ICON_MD_OPEN_IN_NEW, "Open this folder in Windows Explorer"))
        {
        std::string error;
        if (!platform::OpenFolderInExplorer(tab.path, error)) ShowError(ui, error);
        }
}

void DrawViewContents(app::AppState& state, UiState& ui, int tabIndex)
{
    if (!DrawToolbar(state, ui, tabIndex)) return;

    app::Tab& tab = state.tabs[(size_t)tabIndex];
    // The status bar holds buttons now, so it is a frame tall, not a line.
    const float statusHeight = state.settings.showStatusBar
                                   ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 2
                                   : 0.0f;
    const ImVec2 listSize(0, ImGui::GetContentRegionAvail().y - statusHeight);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CurrentPalette().surface);
    ImGui::BeginChild("##listarea", listSize, ImGuiChildFlags_None);
    ImGui::PopStyleColor();
    if (!tab.error.empty())
        {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().danger);
        ImGui::TextWrapped("%s", tab.error.c_str());
        ImGui::PopStyleColor();
        }
    else if (tab.entries.empty() && !tab.path.empty())
        {
        ImGui::Spacing();
        MutedText("This folder is empty.");
        ImGuiWindow* area = ImGui::GetCurrentWindow();
        FileDropTargetRect(state, ui, tab.path, area->InnerRect, area->GetID("##emptydrop"));
        }
    else
        {
        if (tab.viewMode == app::ViewMode::Thumbnails) DrawThumbnailGrid(state, ui, tabIndex);
        else DrawFileTable(state, ui, tabIndex);
        // The list background (and any non-folder row) drops into the folder
        // being shown. Done here at the child level, not inside the table:
        // inside BeginTable the clip rect is a narrow per-column region, so
        // the drop's hover test would miss most of the list.
        if (!tab.path.empty() && !ui.rowDropHovered)
            {
            ImGuiWindow* area = ImGui::GetCurrentWindow();
            FileDropTargetRect(state, ui, tab.path, area->InnerRect, area->GetID("##listdrop"));
            }
        // A press in the empty right-hand margin (outside the table) also
        // begins a rubber-band; the table's own handling promotes and draws
        // it. This is the always-available start point for a packed folder.
        ViewUi& v = ui.view(tab.id);
        ImGuiIO& io = ImGui::GetIO();
        if (!tab.path.empty() && !v.lassoActive && !v.lassoPending && !ImGui::IsDragDropActive() &&
            ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
            v.lassoPending = true;
            v.lassoActive = false;
            v.lassoAnchor = io.MousePos;
            v.lassoAdditive = io.KeyCtrl || io.KeyShift;
            v.lassoBase = tab.selected;
            }
        }
    ImGui::EndChild();
    HandleListKeys(state, ui, tabIndex);

    if (state.settings.showStatusBar) DrawStatusBar(state, ui, state.tabs[(size_t)tabIndex]);
}

} // namespace

bool DrawView(app::AppState& state, UiState& ui, int tabIndex)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);

    if (v.wantFocus)
        {
        ImGui::SetNextWindowFocus();
        v.wantFocus = false;
        }
    if (v.dockHint != 0)
        {
        ImGui::SetNextWindowDockID(v.dockHint, ImGuiCond_Always);
        v.dockHint = 0;
        }
    else if (!v.shown)
        {
        // A brand-new view joins the main area; one restored from the
        // layout file keeps whatever place it had.
        ImGui::SetNextWindowDockID(ui.defaultViewDock, ImGuiCond_FirstUseEver);
        }
    v.shown = true;

    const std::string name = ViewWindowName(state, tab);
    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    const bool visible = ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (visible)
        {
        v.dockId = ImGui::GetWindowDockID();
        // The focused view is the one menus, shortcuts and the navigation
        // pane act on.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
            state.activeTab = tabIndex;
            ui.activeFileView = -1;
            }
        DrawViewContents(state, ui, tabIndex);
        }
    ImGui::End();
    return !open;
}

} // namespace ui
