#include "ui/MainWindow.h"
#include "ui/DragDrop.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/FileOps.h"
#include "app/Listing.h"
#include "app/Navigation.h"
#include "app/Selection.h"
#include "platform/Shell.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace ui
{

void RequestDialog(UiState& ui, Dialog which, const std::string& title)
{
    ui.dialog = which;
    ui.dialogRequested = true;
    ui.dialogTitle = title;
}

void ShowError(UiState& ui, const std::string& message)
{
    ui.dialogMessage = message;
    RequestDialog(ui, Dialog::Error);
}

void ZoomBy(app::AppState& state, float delta)
{
    state.settings.uiScale = ClampUiScale(state.settings.uiScale + delta);
}

void ZoomReset(app::AppState& state)
{
    state.settings.uiScale = 1.0f;
}

namespace
{

void BuildDefaultLayout(ImGuiID dockspaceId, ImVec2 size)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);
    ImGuiID left = 0, right = 0;
    ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.22f, &left, &right);
    ImGui::DockBuilderDockWindow(kNavWindow, left);
    ImGui::DockBuilderDockWindow(kFilesWindow, right);
    ImGui::DockBuilderFinish(dockspaceId);
}

void StartNewFolder(app::AppState& state, UiState& ui)
{
    ui.dialogText = app::UniqueNewName(state.active(), "New folder");
    RequestDialog(ui, Dialog::NewFolder);
}

void StartNewFile(app::AppState& state, UiState& ui)
{
    ui.dialogText = app::UniqueNewName(state.active(), "New Text Document.txt");
    RequestDialog(ui, Dialog::NewFile);
}

void StartRename(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
    int target = tab.focused;
    if (target < 0)
        {
        const std::vector<int> sel = app::SelectedIndices(tab);
        if (sel.size() == 1) target = sel[0];
        }
    if (target < 0 || target >= (int)tab.entries.size() || tab.path.empty()) return;
    ui.dialogEntry = target;
    ui.dialogText = tab.entries[(size_t)target].name;
    RequestDialog(ui, Dialog::Rename);
}

void StartDelete(app::AppState& state, UiState& ui, bool permanent)
{
    app::Tab& tab = state.active();
    if (tab.path.empty()) return;
    const int n = app::SelectedCount(tab);
    if (n == 0) return;
    ui.deletePermanent = permanent;
    if (n == 1)
        {
        const std::vector<int> sel = app::SelectedIndices(tab);
        const platform::FileEntry& e = tab.entries[(size_t)sel[0]];
        ui.dialogMessage = permanent
            ? "Are you sure you want to permanently delete '" + e.name + "'?"
            : "Are you sure you want to move '" + e.name + "' to the Recycle Bin?";
        }
    else
        {
        ui.dialogMessage = permanent
            ? "Are you sure you want to permanently delete these " + std::to_string(n) + " items?"
            : "Are you sure you want to move these " + std::to_string(n) + " items to the Recycle Bin?";
        }
    RequestDialog(ui, Dialog::Delete);
}

void OpenInExplorer(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
    const std::vector<int> sel = app::SelectedIndices(tab);
    std::string error;
    const bool ok = sel.size() == 1 ? platform::ShowInExplorer(tab.entries[(size_t)sel[0]].path, error)
                                    : platform::OpenWithShell(tab.path.empty() ? "shell:MyComputerFolder" : tab.path, error);
    if (!ok) ShowError(ui, error);
}

void DrawMenuBar(app::AppState& state, UiState& ui)
{
    app::Tab& tab = state.active();
    const bool haveSelection = app::SelectedCount(tab) > 0;
    const bool inFolder = !tab.path.empty();
    app::Settings& s = state.settings;

    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
        {
        // Opening or closing a tab moves state.tabs, so `tab` is stale after
        // either; both leave the menu immediately.
        if (ImGui::MenuItem("New tab", "Ctrl+T"))
            {
            app::OpenTab(state, tab.path);
            ImGui::EndMenu();
            ImGui::EndMenuBar();
            return;
            }
        if (ImGui::MenuItem("Close tab", "Ctrl+W"))
            {
            app::CloseTab(state, state.activeTab);
            ImGui::EndMenu();
            ImGui::EndMenuBar();
            return;
            }
        ImGui::Separator();
        if (ImGui::MenuItem("New folder", "Ctrl+Shift+N", false, inFolder)) StartNewFolder(state, ui);
        if (ImGui::MenuItem("New text file", nullptr, false, inFolder)) StartNewFile(state, ui);
        ImGui::Separator();
        if (ImGui::MenuItem("Open in Windows Explorer")) OpenInExplorer(state, ui);
        if (ImGui::MenuItem("Open terminal here", nullptr, false, inFolder))
            {
            std::string error;
            if (!platform::OpenTerminalAt(tab.path, error)) ShowError(ui, error);
            }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) state.quitRequested = true;
        ImGui::EndMenu();
        }

    if (ImGui::BeginMenu("Edit"))
        {
        std::string clipError;
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, haveSelection && inFolder) && !app::CopySelection(state, tab, true, clipError)) ShowError(ui, clipError);
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, haveSelection && inFolder) && !app::CopySelection(state, tab, false, clipError)) ShowError(ui, clipError);
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, app::CanPaste(state, tab)))
            {
            std::string error;
            if (!app::Paste(state, tab, error)) ShowError(ui, error);
            }
        ImGui::Separator();
        if (ImGui::MenuItem("Select all", "Ctrl+A")) app::SelectAll(tab);
        if (ImGui::MenuItem("Select none")) app::ClearSelection(tab);
        if (ImGui::MenuItem("Invert selection")) app::InvertSelection(tab);
        ImGui::Separator();
        if (ImGui::MenuItem("Rename", "F2", false, haveSelection && inFolder)) StartRename(state, ui);
        if (ImGui::MenuItem("Delete", "Del", false, haveSelection && inFolder)) StartDelete(state, ui, false);
        if (ImGui::MenuItem("Delete permanently", "Shift+Del", false, haveSelection && inFolder)) StartDelete(state, ui, true);
        ImGui::EndMenu();
        }

    if (ImGui::BeginMenu("View"))
        {
        ImGui::MenuItem("Navigation pane", nullptr, &s.showNavPane);
        ImGui::MenuItem("Status bar", nullptr, &s.showStatusBar);
        ImGui::Separator();
        if (ImGui::MenuItem("Hidden items", nullptr, &s.showHidden))
            {
            for (app::Tab& t : state.tabs) app::ApplyView(t, s);
            ui.treeChildren.clear();
            }
        ImGui::MenuItem("File name extensions", nullptr, &s.showExtensions);
        ImGui::Separator();
        if (ImGui::BeginMenu("Sort by"))
            {
            const char* names[] = { "Name", "Date modified", "Type", "Size" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::MenuItem(names[i], nullptr, (int)tab.sort.column == i))
                    {
                    tab.sort.column = (app::SortColumn)i;
                    app::ApplyView(tab, s);
                    }
            ImGui::Separator();
            if (ImGui::MenuItem("Ascending", nullptr, tab.sort.ascending))  { tab.sort.ascending = true;  app::ApplyView(tab, s); }
            if (ImGui::MenuItem("Descending", nullptr, !tab.sort.ascending)) { tab.sort.ascending = false; app::ApplyView(tab, s); }
            ImGui::EndMenu();
            }
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom in", "Ctrl++")) ZoomBy(state, 0.1f);
        if (ImGui::MenuItem("Zoom out", "Ctrl+-")) ZoomBy(state, -0.1f);
        if (ImGui::MenuItem("Reset zoom", "Ctrl+0")) ZoomReset(state);
        ImGui::Separator();
        if (ImGui::BeginMenu("Theme"))
            {
            for (int i = 0; i < ThemeCount(); ++i)
                if (ImGui::MenuItem(ThemeAt(i).name, nullptr, s.themeIndex == i)) s.themeIndex = i;
            ImGui::EndMenu();
            }
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "F5"))
            {
            app::Refresh(tab);
            state.drives = platform::GetDrives();
            ui.treeChildren.clear();
            }
        if (ImGui::MenuItem("Reset layout")) ui.resetLayoutRequested = true;
        ImGui::EndMenu();
        }

    if (ImGui::BeginMenu("Go"))
        {
        if (ImGui::MenuItem("Back", "Alt+Left", false, app::CanGoBack(tab))) app::GoBack(tab);
        if (ImGui::MenuItem("Forward", "Alt+Right", false, app::CanGoForward(tab))) app::GoForward(tab);
        if (ImGui::MenuItem("Up", "Alt+Up", false, app::CanGoUp(tab))) app::GoUp(tab);
        ImGui::Separator();
        std::string error;
        if (ImGui::MenuItem("This PC")) app::NavigateTo(tab, "", error);
        for (const platform::KnownFolder& kf : state.quickAccess)
            if (ImGui::MenuItem(kf.name.c_str())) app::NavigateTo(tab, kf.path, error);
        ImGui::EndMenu();
        }

    if (ImGui::BeginMenu("Help"))
        {
        if (ImGui::MenuItem("About Davesplorer")) RequestDialog(ui, Dialog::About);
        ImGui::Separator();
        ImGui::MenuItem("ImGui metrics", nullptr, &ui.showMetrics);
        ImGui::MenuItem("ImGui demo", nullptr, &ui.showDemo);
        ImGui::EndMenu();
        }
    ImGui::EndMenuBar();
}

// Shortcuts that apply wherever focus is, as long as nobody is typing and no
// popup is up.
void HandleGlobalShortcuts(app::AppState& state, UiState& ui)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;
    app::Tab& tab = state.active();

    auto chord = [](ImGuiKeyChord c) { return ImGui::IsKeyChordPressed(c); };

    // These two move state.tabs and invalidate `tab`; nothing else runs
    // in the same frame.
    if (chord(ImGuiMod_Ctrl | ImGuiKey_T))
        {
        app::OpenTab(state, tab.path);
        return;
        }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_W))
        {
        app::CloseTab(state, state.activeTab);
        return;
        }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Tab) || chord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Tab))
        {
        const int n = (int)state.tabs.size();
        const int dir = io.KeyShift ? -1 : 1;
        const int next = ((state.activeTab + dir) % n + n) % n;
        ui.selectTabId = state.tabs[(size_t)next].id;
        }
    if (chord(ImGuiMod_Alt | ImGuiKey_LeftArrow)) app::GoBack(tab);
    if (chord(ImGuiMod_Alt | ImGuiKey_RightArrow)) app::GoForward(tab);
    if (chord(ImGuiMod_Alt | ImGuiKey_UpArrow)) app::GoUp(tab);
    if (chord(ImGuiKey_F5))
        {
        app::Refresh(tab);
        ui.treeChildren.clear();
        }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_L) || chord(ImGuiMod_Alt | ImGuiKey_D))
        {
        ui.addressEditing = true;
        ui.addressWantFocus = true;
        ui.addressText = tab.path;
        }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_F) || chord(ImGuiMod_Ctrl | ImGuiKey_E)) ui.searchWantFocus = true;
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Equal) || chord(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd)) ZoomBy(state, 0.1f);
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Minus) || chord(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract)) ZoomBy(state, -0.1f);
    if (chord(ImGuiMod_Ctrl | ImGuiKey_0) || chord(ImGuiMod_Ctrl | ImGuiKey_Keypad0)) ZoomReset(state);
    if (chord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N) && !tab.path.empty()) StartNewFolder(state, ui);
    if (io.KeyCtrl && io.MouseWheel != 0.0f) ZoomBy(state, io.MouseWheel > 0 ? 0.1f : -0.1f);
}

} // namespace

void DrawFrame(app::AppState& state, UiState& ui)
{
    BeginDragDropFrame(state, ui);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    const ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(kHostWindow, nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    DrawMenuBar(state, ui);

    const ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
    if (ui.resetLayoutRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
        {
        ui.resetLayoutRequested = false;
        BuildDefaultLayout(dockspaceId, viewport->WorkSize);
        }
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_AutoHideTabBar);
    ImGui::End();

    if (state.settings.showNavPane)
        {
        bool open = true;
        if (ImGui::Begin(kNavWindow, &open, ImGuiWindowFlags_NoCollapse)) DrawNavPane(state, ui);
        ImGui::End();
        if (!open) state.settings.showNavPane = false;
        }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    if (ImGui::Begin(kFilesWindow, nullptr, ImGuiWindowFlags_NoCollapse)) DrawFilesPane(state, ui);
    ImGui::End();
    ImGui::PopStyleVar();

    // Dialogs are opened and drawn in the host window so OpenPopup and
    // BeginPopupModal see the same ID stack, whichever pane asked.
    ImGui::Begin(kHostWindow);
    DrawDialogs(state, ui);
    ImGui::End();

    if (ui.showMetrics) ImGui::ShowMetricsWindow(&ui.showMetrics);
    if (ui.showDemo) ImGui::ShowDemoWindow(&ui.showDemo);

    HandleGlobalShortcuts(state, ui);
}

} // namespace ui
