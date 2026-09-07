#include "ui/MainWindow.h"
#include "ui/Fonts.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/FileOps.h"
#include "app/Format.h"
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
#include <string>

namespace ui
{

namespace
{

void Navigate(app::AppState& state, UiState& ui, const std::string& path)
{
    std::string error;
    if (!app::NavigateTo(state.active(), path, error)) ShowError(ui, error);
}

void OpenSelectedOrFocused(app::AppState& state, UiState& ui)
{
    // Copy what we need first: opening a tab reallocates state.tabs and
    // would leave a reference into the old vector dangling.
    std::vector<int> targets;
    std::vector<std::string> extraFolders;
    {
    const app::Tab& tab = state.active();
    targets = app::SelectedIndices(tab);
    if (targets.empty() && tab.focused >= 0) targets.push_back(tab.focused);
    // Enter on several folders: the first opens here, the others each get
    // a tab, the way Explorer would give each its own window.
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
        if (!app::OpenEntry(state, state.active(), i, error)) ShowError(ui, error);
    for (const std::string& folder : extraFolders) app::OpenTab(state, folder);
}

void DrawTabBar(app::AppState& state, UiState& ui)
{
    const ImGuiTabBarFlags flags = ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll |
                                   ImGuiTabBarFlags_NoTooltip;
    if (!ImGui::BeginTabBar("Tabs", flags)) return;
    int closeIndex = -1;
    for (int i = 0; i < (int)state.tabs.size(); ++i)
        {
        app::Tab& t = state.tabs[(size_t)i];
        const std::string label = app::LocationTitle(state, t.path) + "###tab" + std::to_string(t.id);
        bool open = true;
        ImGuiTabItemFlags itemFlags = 0;
        if (ui.selectTabId == t.id) itemFlags |= ImGuiTabItemFlags_SetSelected;
        if (ImGui::BeginTabItem(label.c_str(), &open, itemFlags))
            {
            state.activeTab = i;
            ImGui::EndTabItem();
            }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !t.path.empty()) ImGui::SetTooltip("%s", t.path.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) open = false;
        if (!open) closeIndex = i;
        }
    ui.selectTabId = -1;
    if (ImGui::TabItemButton(ICON_MD_ADD "###NewTab", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
        app::OpenTab(state, state.active().path);
    ImGui::EndTabBar();
    if (closeIndex >= 0) app::CloseTab(state, closeIndex);
}

void DrawAddressBar(app::AppState& state, UiState& ui, float width)
{
    app::Tab& tab = state.active();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight();

    if (ui.addressEditing)
        {
        ImGui::SetNextItemWidth(width);
        if (ui.addressWantFocus)
            {
            ImGui::SetKeyboardFocusHere();
            ui.addressWantFocus = false;
            }
        const bool entered = ImGui::InputText("##address", &ui.addressText,
                                              ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool active = ImGui::IsItemActive();
        if (entered)
            {
            ui.addressEditing = false;
            std::string error;
            if (!app::NavigateTo(tab, ui.addressText, error)) ShowError(ui, error);
            }
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (!active && !ImGui::IsItemActivated() && ImGui::IsItemDeactivated()))
            {
            ui.addressEditing = false;
            }
        else if (!active && !ImGui::IsItemFocused() && !ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
            // Clicked somewhere else: leave edit mode, keep the old path.
            ui.addressEditing = false;
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
        ImGui::SameLine();
        }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // The rest of the bar: clicking it turns the crumbs into a text field.
    const float remaining = std::max(ImGui::GetContentRegionAvail().x, editArea * 0.5f);
    if (ImGui::InvisibleButton("###addressclick", ImVec2(remaining, height)))
        {
        ui.addressEditing = true;
        ui.addressWantFocus = true;
        ui.addressText = tab.path;
        }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    ImGui::EndChild();
    if (haveClick) Navigate(state, ui, clicked);
}

void DrawToolbar(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
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

    const float searchWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.25f, 120.0f, 320.0f);
    const float addressWidth = ImGui::GetContentRegionAvail().x - searchWidth - ImGui::GetStyle().ItemSpacing.x;
    DrawAddressBar(state, ui, std::max(addressWidth, 80.0f));
    ImGui::SameLine();

    ImGui::SetNextItemWidth(searchWidth);
    if (ui.searchWantFocus)
        {
        ImGui::SetKeyboardFocusHere();
        ui.searchWantFocus = false;
        }
    const std::string hint = std::string(ICON_MD_SEARCH) + " Search " + app::LocationTitle(state, tab.path);
    if (ImGui::InputTextWithHint("##search", hint.c_str(), &tab.filter)) app::ApplyView(tab, state.settings);
    if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape) && !tab.filter.empty())
        {
        tab.filter.clear();
        app::ApplyView(tab, state.settings);
        }
}

void DrawRowContextMenu(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
    const bool inFolder = !tab.path.empty();
    const bool haveSelection = app::SelectedCount(tab) > 0;
    const bool focusedIsFolder = tab.focused >= 0 && tab.entries[(size_t)tab.focused].isDirectory;
    const std::string focusedPath = tab.focused >= 0 ? tab.entries[(size_t)tab.focused].path : std::string();
    if (ImGui::MenuItem("Open"))
        {
        OpenSelectedOrFocused(state, ui);
        return;   // tabs may have changed; `tab` is not to be trusted past here
        }
    if (ImGui::MenuItem("Open in new tab", nullptr, false, focusedIsFolder))
        {
        app::OpenTab(state, focusedPath);
        return;
        }
    if (ImGui::MenuItem("Show in Windows Explorer"))
        {
        std::string error;
        const std::vector<int> sel = app::SelectedIndices(tab);
        if (!sel.empty() && !platform::ShowInExplorer(tab.entries[(size_t)sel[0]].path, error)) ShowError(ui, error);
        }
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X", false, haveSelection && inFolder)) app::CopySelection(state, tab, true);
    if (ImGui::MenuItem("Copy", "Ctrl+C", false, haveSelection && inFolder)) app::CopySelection(state, tab, false);
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

void DrawBackgroundContextMenu(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
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

// Keys that act on the list. Only while the Files window (or a child of it)
// has focus, nobody is typing, and no popup is up.
void HandleListKeys(app::AppState& state, UiState& ui)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    app::Tab& tab = state.active();
    const bool shift = io.KeyShift, ctrl = io.KeyCtrl;
    const int before = tab.focused;

    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  app::MoveFocus(tab, 1, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    app::MoveFocus(tab, -1, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))   app::MoveFocus(tab, 15, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))     app::MoveFocus(tab, -15, false, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_Home))       app::MoveFocus(tab, -1, true, shift, ctrl);
    if (ImGui::IsKeyPressed(ImGuiKey_End))        app::MoveFocus(tab, 1, true, shift, ctrl);
    if (tab.focused != before) ui.scrollToEntry = tab.focused;

    if (ImGui::IsKeyPressed(ImGuiKey_Space) && ctrl && tab.focused >= 0)
        tab.selected[(size_t)tab.focused] = tab.selected[(size_t)tab.focused] ? 0 : 1;
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        {
        OpenSelectedOrFocused(state, ui);
        return;   // may have opened tabs and moved state.tabs
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
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && !tab.path.empty()) app::CopySelection(state, tab, false);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X, false) && !tab.path.empty()) app::CopySelection(state, tab, true);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && app::CanPaste(state, tab))
        {
        std::string error;
        if (!app::Paste(state, tab, error)) ShowError(ui, error);
        }
}

void DrawFileTable(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
    const app::Settings& settings = state.settings;
    const float scale = ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi;

    const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                                  ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("FileTable", 4, flags)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide |
                                    ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending, 1.0f, 0);
    ImGui::TableSetupColumn("Date modified", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 150.0f * scale, 1);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 130.0f * scale, 2);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 90.0f * scale, 3);
    ImGui::TableHeadersRow();

    // The table's sort state is the source of truth; the tab follows it.
    // Comparing every frame also covers a tab switch, where the table keeps
    // its spec and the new tab must adopt it.
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

    if (ui.seenGeneration != tab.listingGeneration)
        {
        ui.seenGeneration = tab.listingGeneration;
        ImGui::SetScrollY(0.0f);
        }

    const ImVec4 selectedBg = ImGui::ColorConvertU32ToFloat4(WithAlpha(CurrentPalette().accent, 70));
    const ImVec4 hoveredBg  = ImGui::ColorConvertU32ToFloat4(WithAlpha(CurrentPalette().accent, 40));
    ImGui::PushStyleColor(ImGuiCol_Header, selectedBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoveredBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, selectedBg);

    bool rowHovered = false;
    int pendingClick = -1;
    bool pendingDouble = false;
    int pendingContext = -1;

    ImGuiListClipper clipper;
    clipper.Begin((int)tab.visible.size());
    if (ui.scrollToEntry >= 0)
        {
        const int pos = app::VisiblePosOf(tab, ui.scrollToEntry);
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

            std::string shownName = e.name;
            if (!settings.showExtensions && !e.isDirectory && !e.extension.empty() && tab.path.size() > 0)
                shownName = e.name.substr(0, e.name.size() - e.extension.size() - 1);

            if (ui.scrollToEntry == entryIndex)
                {
                ImGui::SetScrollHereY(0.5f);
                ui.scrollToEntry = -1;
                }

            // The selectable carries only the ID ("###name" shows nothing),
            // so the icon can be drawn in its own tint and the name in text
            // color on top of it. The ID is the file name, which is what the
            // tests refer to.
            const std::string label = "###" + e.name;
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Selectable(label.c_str(), isSelected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap);
            const char* icon = IconForEntry(e);
            const float iconW = ImGui::CalcTextSize(icon).x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(rowStart, IconTintForEntry(e), icon);
            dl->AddText(ImVec2(rowStart.x + iconW + ImGui::GetStyle().ItemInnerSpacing.x, rowStart.y),
                        e.isHidden ? CurrentPalette().textFaint : CurrentPalette().text, shownName.c_str());

            if (ImGui::IsItemHovered()) rowHovered = true;
            if (clicked)
                {
                pendingClick = pos;
                pendingDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) pendingContext = pos;
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

    // Resolve clicks after the loop so selection changes cannot shift rows
    // mid-iteration.
    ImGuiIO& io = ImGui::GetIO();
    if (pendingContext >= 0)
        {
        const int entryIndex = tab.visible[(size_t)pendingContext];
        if (!(entryIndex < (int)tab.selected.size() && tab.selected[(size_t)entryIndex]))
            app::ClickSelect(tab, pendingContext, false, false);
        tab.focused = entryIndex;
        ImGui::OpenPopup("RowContext");
        }
    else if (pendingClick >= 0)
        {
        if (pendingDouble)
            {
            app::ClickSelect(tab, pendingClick, false, false);
            std::string error;
            if (!app::OpenEntry(state, tab, tab.visible[(size_t)pendingClick], error)) ShowError(ui, error);
            }
        else
            {
            app::ClickSelect(tab, pendingClick, io.KeyCtrl, io.KeyShift);
            }
        }
    else if (ImGui::IsWindowHovered() && !rowHovered && !ImGui::IsAnyItemHovered())
        {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) app::ClearSelection(tab);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("BackgroundContext");
        }

    if (ImGui::BeginPopup("RowContext"))
        {
        DrawRowContextMenu(state, ui);
        ImGui::EndPopup();
        }
    if (ImGui::BeginPopup("BackgroundContext"))
        {
        DrawBackgroundContextMenu(state, ui);
        ImGui::EndPopup();
        }
    ImGui::EndTable();
}

void DrawStatusBar(app::AppState& state)
{
    const app::Tab& tab = state.active();
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
    MutedText(text.c_str());
    if (!state.statusMessage.empty())
        {
        ImGui::SameLine();
        FaintText(state.statusMessage.c_str());
        }
}

} // namespace

void DrawFilesPane(app::AppState& state, UiState& ui)
{
    DrawTabBar(state, ui);
    DrawToolbar(state, ui);

    app::Tab& tab = state.active();
    const float statusHeight = state.settings.showStatusBar
                                   ? ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2
                                   : 0.0f;
    const ImVec2 listSize(0, ImGui::GetContentRegionAvail().y - statusHeight);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, CurrentPalette().surface);
    ImGui::BeginChild("##listarea", listSize, ImGuiChildFlags_None);
    ImGui::PopStyleColor();
    if (ui.filesWantFocus)
        {
        ImGui::SetWindowFocus();
        ui.filesWantFocus = false;
        }
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
        }
    else
        {
        DrawFileTable(state, ui);
        }
    ImGui::EndChild();
    HandleListKeys(state, ui);

    if (state.settings.showStatusBar) DrawStatusBar(state);
}

} // namespace ui
