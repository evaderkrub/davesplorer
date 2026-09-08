// End-to-end tests: the real interface driven by the Dear ImGui Test Engine,
// headless. No window is created; ImGui runs against a fixed display size
// and a stand-in renderer that only acknowledges texture requests.
//
// Exit code is 0 when every test passed.

#include "app/AppState.h"
#include "app/Listing.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "app/FileOps.h"
#include "app/Shortcuts.h"
#include "platform/Clipboard.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"
#include "ui/Fonts.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/UiState.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_coroutine.h"
#include "imgui_test_engine/imgui_te_engine.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <SDL3/SDL.h>
#endif
#include <filesystem>
#undef Yield   // windows.h defines Yield() as a macro, which breaks ctx->Yield()

#include <cstdio>
#include <fstream>
#include <string>

namespace
{

struct Fixture
{
    app::AppState state;
    ui::UiState   ui;
    std::string   root;      // scratch folder with a known layout
    std::string   exeDir;    // where settings.ini and imgui.ini would go
};

Fixture* g_fx = nullptr;

std::string TempRoot(const char* tag)
{
#ifdef _WIN32
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    return platform::WideToUtf8(tmp) + "davesplorer_" + tag + "_" + std::to_string(GetCurrentProcessId());
#else
    return (std::filesystem::temp_directory_path() / ("davesplorer_" + std::string(tag) + "_" + std::to_string(getpid()))).string();
#endif
}

void WriteFile(const std::string& path, const char* content)
{
    std::ofstream(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str()))) << content;
}

// The listing for the scratch folder, freshly read from disk.
void ResetScratch(Fixture& fx)
{
    std::string error;
    platform::DeletePermanently({ fx.root }, error);
    platform::CreateFolder(fx.root, error);
    WriteFile(app::JoinPath(fx.root, "alpha.txt"), "alpha");
    WriteFile(app::JoinPath(fx.root, "beta.md"), "beta beta");
    WriteFile(app::JoinPath(fx.root, "gamma.png"), "not really a png");
    platform::CreateFolder(app::JoinPath(fx.root, "sub"), error);
    WriteFile(app::JoinPath(app::JoinPath(fx.root, "sub"), "inner.txt"), "inner");
}

// Each view is a window named "<folder>###view<id>"; the ID part is what
// the tests address, so the title can change with navigation.
ImGuiTestRef ActiveViewRef(Fixture& fx)
{
    return ImGuiTestRef(ui::ViewWindowId(fx.state.active().id));
}

void RefActiveView(ImGuiTestContext* ctx, Fixture& fx)
{
    ctx->SetRef(ActiveViewRef(fx));
}

// Every test starts from one view in the scratch folder.
void GoToScratch(ImGuiTestContext* ctx, Fixture& fx)
{
    ResetScratch(fx);
    fx.state.tabs.clear();
    fx.state.activeTab = 0;
    app::OpenTab(fx.state, fx.root);
    fx.state.settings.uiScale = 1.0f;
    fx.state.settings.showHidden = false;
    fx.ui = ui::UiState{};
    ctx->Yield(3);
}

// A stand-in for the renderer's texture upload: ImGui asks for textures to
// be created and updated; here they are simply acknowledged.
void AcknowledgeTextures()
{
    ImDrawData* dd = ImGui::GetDrawData();
    if (!dd || !dd->Textures) return;
    for (ImTextureData* tex : *dd->Textures)
        {
        if (tex->Status == ImTextureStatus_WantCreate)
            {
            tex->SetTexID((ImTextureID)1);
            tex->SetStatus(ImTextureStatus_OK);
            }
        else if (tex->Status == ImTextureStatus_WantUpdates)
            {
            tex->SetStatus(ImTextureStatus_OK);
            }
        else if (tex->Status == ImTextureStatus_WantDestroy)
            {
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
            }
        }
}

void RegisterTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    t = IM_REGISTER_TEST(e, "explorer", "startup_windows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        IM_CHECK(ctx->WindowInfo(ActiveViewRef(fx)).Window != nullptr);
        IM_CHECK(ctx->WindowInfo(ui::kNavWindow).Window != nullptr);
        IM_CHECK(ctx->WindowInfo(ui::kHostWindow).Window != nullptr);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)1);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), fx.root.c_str());
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)4);
    };

    t = IM_REGISTER_TEST(e, "explorer", "address_bar_navigates");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###addressclick");
        ctx->Yield();
        IM_CHECK(fx.ui.view(fx.state.active().id).addressEditing);
        const std::string sub = app::JoinPath(fx.root, "sub");
        ctx->ItemInputValue("**/##address", sub.c_str());
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)1);
        IM_CHECK(!fx.ui.view(fx.state.active().id).addressEditing);

        // A bad path shows the error modal and leaves the tab where it is.
        ctx->ItemClick("**/###addressclick");
        ctx->ItemInputValue("**/##address", "C:\\this\\does\\not\\exist\\anywhere");
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());
        ctx->SetRef("//Error");
        ctx->ItemClick("OK");
        ctx->Yield();
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
    };

    t = IM_REGISTER_TEST(e, "explorer", "double_click_back_up");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemDoubleClick("**/###sub");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), app::JoinPath(fx.root, "sub").c_str());
        ctx->ItemClick("**/###Back");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), fx.root.c_str());
        ctx->ItemClick("**/###Forward");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), app::JoinPath(fx.root, "sub").c_str());
        ctx->ItemClick("**/###Up");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), fx.root.c_str());
        // Breadcrumb: the first crumb is This PC.
        // Breadcrumb: the last crumb is the current folder, the one before
        // it the parent; a deep temp path collapses the leading crumbs, so
        // the "..." menu must offer This PC.
        const size_t n = app::Breadcrumbs(fx.root).size();
        ctx->ItemClick(("**/###crumb" + std::to_string(n - 2)).c_str());
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), app::ParentPath(fx.root).c_str());
        if (ctx->ItemExists("**/###crumbmore"))
            {
            ctx->ItemClick("**/###crumbmore");
            ctx->Yield();
            ctx->ItemClick("//$FOCUSED/###crumbmenu0");
            }
        else
            {
            ctx->ItemClick("**/###crumb0");
            }
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), "");
        IM_CHECK(!fx.state.active().entries.empty());
    };

    t = IM_REGISTER_TEST(e, "explorer", "search_filters_list");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemInputValue("**/##search", "ALP");
        ctx->Yield();
        const app::Tab& tab = fx.state.active();
        IM_CHECK_EQ(tab.visible.size(), (size_t)1);
        IM_CHECK_STR_EQ(tab.entries[(size_t)tab.visible[0]].name.c_str(), "alpha.txt");
        ctx->ItemInputValue("**/##search", "");
        ctx->Yield();
        IM_CHECK_EQ(fx.state.active().visible.size(), (size_t)4);
    };

    t = IM_REGISTER_TEST(e, "explorer", "selection_click_ctrl_shift");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###alpha.txt");
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 1);
        ctx->KeyDown(ImGuiMod_Ctrl);
        ctx->ItemClick("**/###gamma.png");
        ctx->KeyUp(ImGuiMod_Ctrl);
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 2);
        ctx->KeyDown(ImGuiMod_Shift);
        ctx->ItemClick("**/###sub");
        ctx->KeyUp(ImGuiMod_Shift);
        // Shift from the anchor (gamma) up to sub covers every row.
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 4);
        ctx->ItemClick("**/###beta.md");
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 1);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_A);
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 4);
        // Arrow keys move a single selection.
        ctx->ItemClick("**/###sub");
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->KeyPress(ImGuiKey_DownArrow);
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 1);
        IM_CHECK_STR_EQ(fx.state.active().entries[(size_t)fx.state.active().focused].name.c_str(), "beta.md");
    };

    t = IM_REGISTER_TEST(e, "explorer", "sort_by_column_header");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/Size");
        ctx->Yield();
        IM_CHECK(fx.state.active().sort.column == app::SortColumn::Size);
        const app::Tab& tab = fx.state.active();
        // Folders stay first; the biggest file follows.
        IM_CHECK(tab.entries[(size_t)tab.visible[0]].isDirectory);
        IM_CHECK_STR_EQ(tab.entries[(size_t)tab.visible[1]].name.c_str(), "gamma.png");
        ctx->ItemClick("**/Name");
        ctx->Yield();
        IM_CHECK(fx.state.active().sort.column == app::SortColumn::Name);
    };

    t = IM_REGISTER_TEST(e, "explorer", "about_is_modal");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("Help/About Davesplorer");
        ctx->Yield(2);
        ImGuiWindow* modal = ImGui::GetTopMostPopupModal();
        IM_CHECK(modal != nullptr);
        IM_CHECK_STR_EQ(modal->Name, "About Davesplorer");
        ctx->SetRef("//About Davesplorer");
        ctx->ItemClick("OK");
        ctx->Yield();
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
    };

    t = IM_REGISTER_TEST(e, "explorer", "zoom_scales_interface");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        const float before = ImGui::GetStyle().FontScaleMain;
        const float paddingBefore = ImGui::GetStyle().FramePadding.x;
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("View/Zoom in");
        ctx->Yield(2);
        IM_CHECK(fx.state.settings.uiScale > 1.05f);
        IM_CHECK(ImGui::GetStyle().FontScaleMain > before);
        // Metrics are truncated to whole pixels, so one step may not move
        // an 8px padding; five steps (150%) must.
        for (int i = 0; i < 4; ++i) ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Equal);
        ctx->Yield(2);
        IM_CHECK(fx.state.settings.uiScale > 1.45f);
        IM_CHECK(ImGui::GetStyle().FramePadding.x > paddingBefore);
        for (int i = 0; i < 5; ++i) ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Minus);
        ctx->Yield(2);
        IM_CHECK(fx.state.settings.uiScale > 0.95f && fx.state.settings.uiScale < 1.05f);
        ctx->MenuClick("View/Zoom out");
        ctx->Yield(2);
        IM_CHECK(fx.state.settings.uiScale < 0.95f);
        ctx->MenuClick("View/Reset zoom");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.settings.uiScale, 1.0f);
        IM_CHECK_EQ(ImGui::GetStyle().FramePadding.x, paddingBefore);
    };

    t = IM_REGISTER_TEST(e, "explorer", "theme_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("View/Theme/Light");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.settings.themeIndex, ui::ThemeIndexByName("Light"));
        IM_CHECK_EQ(ui::CurrentThemeIndex(), ui::ThemeIndexByName("Light"));
        IM_CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x > 0.9f);
        ctx->MenuClick("View/Theme/Wili Dark");
        ctx->Yield(2);
        IM_CHECK_EQ(ui::CurrentThemeIndex(), 0);
        IM_CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x < 0.2f);
    };

    t = IM_REGISTER_TEST(e, "explorer", "views_open_close");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        const int firstId = fx.state.active().id;
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        // The new view is focused, so it is the active one, and it joined
        // the first view's dock node as a tab.
        IM_CHECK_EQ(fx.state.activeTab, 1);
        IM_CHECK(fx.ui.view(fx.state.active().id).dockId != 0);
        IM_CHECK_EQ(fx.ui.view(fx.state.active().id).dockId, fx.ui.view(firstId).dockId);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###NewTab");
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)3);
        // Ctrl+Tab cycles focus, wrapping around to the first view.
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Tab);
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.active().id, firstId);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_W);
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("File/Close view");
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)1);
        IM_CHECK(!fx.state.quitRequested);
    };

    t = IM_REGISTER_TEST(e, "explorer", "split_view_side_by_side");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        const int leftId = fx.state.active().id;
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("View/Split view right");
        ctx->Yield(4);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        const int rightId = fx.state.tabs[1].id;
        IM_CHECK_STR_EQ(fx.state.tabs[1].path.c_str(), fx.root.c_str());
        // Both views are docked, in different nodes, and both are visible
        // at once: a real side-by-side layout, not two tabs.
        const ImGuiID leftNode = fx.ui.view(leftId).dockId;
        const ImGuiID rightNode = fx.ui.view(rightId).dockId;
        IM_CHECK(leftNode != 0 && rightNode != 0);
        IM_CHECK(leftNode != rightNode);
        ImGuiWindow* leftWin = ctx->WindowInfo(ImGuiTestRef(ui::ViewWindowId(leftId))).Window;
        ImGuiWindow* rightWin = ctx->WindowInfo(ImGuiTestRef(ui::ViewWindowId(rightId))).Window;
        IM_CHECK(leftWin != nullptr && rightWin != nullptr);
        IM_CHECK(leftWin->Active && rightWin->Active);
        IM_CHECK(rightWin->Pos.x > leftWin->Pos.x);

        // Drag a file from the left view onto a folder row in the right view.
        ctx->SetRef(ImGuiTestRef(ui::ViewWindowId(leftId)));
        const ImGuiID src = ctx->ItemInfo("**/###alpha.txt").ID;
        IM_CHECK(src != 0);
        ctx->SetRef(ImGuiTestRef(ui::ViewWindowId(rightId)));
        ctx->ItemDragAndDrop(ImGuiTestRef(src), "**/###sub");
        ctx->Yield(3);
        const std::string sub = app::JoinPath(fx.root, "sub");
        IM_CHECK(platform::PathExists(app::JoinPath(sub, "alpha.txt")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "alpha.txt")));
        // Both views re-read the folder.
        IM_CHECK_EQ(fx.state.tabs[0].entries.size(), (size_t)3);
        IM_CHECK_EQ(fx.state.tabs[1].entries.size(), (size_t)3);

        // Split down from the right view: three views, three nodes.
        fx.ui.view(rightId).wantFocus = true;
        ctx->Yield(2);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_DownArrow);
        ctx->Yield(4);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)3);
        const ImGuiID thirdNode = fx.ui.view(fx.state.tabs[2].id).dockId;
        IM_CHECK(thirdNode != 0 && thirdNode != leftNode && thirdNode != rightNode);
    };

    t = IM_REGISTER_TEST(e, "explorer", "drag_between_views_into_folder");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        const int leftId = fx.state.active().id;
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("View/Split view right");
        ctx->Yield(4);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        const int rightId = fx.state.tabs[1].id;
        // Point the right view at sub, so the two views show different
        // folders -- the real "drag from here to there" case.
        std::string err;
        app::NavigateTo(fx.state.tabs[1], app::JoinPath(fx.root, "sub"), err);
        ctx->Yield(3);
        const std::string sub = app::JoinPath(fx.root, "sub");
        IM_CHECK_STR_EQ(fx.state.tabs[1].path.c_str(), sub.c_str());

        // A real mouse drag (press, cross to the other window, release), not
        // the teleporting ItemDragAndDrop -- dropped onto the right view's
        // list background, i.e. into the folder it is showing.
        ctx->SetRef(ImGuiTestRef(ui::ViewWindowId(leftId)));
        ctx->MouseMove("**/###alpha.txt");
        ctx->MouseDown(0);
        ctx->MouseLiftDragThreshold();
        ImGuiWindow* rightWin = ctx->WindowInfo(ImGuiTestRef(ui::ViewWindowId(rightId))).Window;
        IM_CHECK(rightWin != nullptr);
        ctx->MouseMoveToPos(ImVec2(rightWin->Pos.x + rightWin->Size.x * 0.5f,
                                   rightWin->Pos.y + rightWin->Size.y * 0.7f));
        ctx->Yield(2);
        IM_CHECK(ImGui::IsDragDropActive());
        IM_CHECK_EQ(fx.ui.dragPaths.size(), (size_t)1);
        ctx->MouseUp(0);
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(sub, "alpha.txt")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "alpha.txt")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "new_folder_dialog");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("File/New folder");
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        ctx->SetRef("//New folder");
        ctx->ItemInputValue("##name", "Created here");
        ctx->Yield(3);
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
        IM_CHECK(platform::IsDirectory(app::JoinPath(fx.root, "Created here")));
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)5);

        // The status bar's New folder button opens the same dialog.
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###NewFolder");
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        ctx->SetRef("//New folder");
        ctx->ItemInputValue("##name", "From the bar");
        ctx->Yield(3);
        IM_CHECK(platform::IsDirectory(app::JoinPath(fx.root, "From the bar")));
        // The terminal button is there and enabled in a real folder; it is
        // not clicked, since that would open a console window.
        RefActiveView(ctx, fx);
        IM_CHECK(ctx->ItemExists("**/###Terminal"));
        IM_CHECK((ctx->ItemInfo("**/###Terminal").ItemFlags & ImGuiItemFlags_Disabled) == 0);
        // Same for the Explorer button, which opens a real Explorer window.
        IM_CHECK(ctx->ItemExists("**/###Explorer"));
        IM_CHECK((ctx->ItemInfo("**/###Explorer").ItemFlags & ImGuiItemFlags_Disabled) == 0);

        // An invalid name keeps the dialog open with a reason.
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("File/New folder");
        ctx->Yield(2);
        ctx->SetRef("//New folder");
        ctx->ItemInputValue("##name", "bad/name");
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        IM_CHECK(!fx.ui.dialogMessage.empty());
        ctx->ItemClick("Cancel");
        ctx->Yield();
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
    };

    t = IM_REGISTER_TEST(e, "explorer", "rename_with_f2");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###alpha.txt");
        ctx->KeyPress(ImGuiKey_F2);
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        ctx->SetRef("//Rename");
        ctx->ItemInputValue("##name", "omega.txt");
        ctx->Yield(3);
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
        IM_CHECK(platform::PathExists(app::JoinPath(fx.root, "omega.txt")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "alpha.txt")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "delete_permanently");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###beta.md");
        ctx->KeyPress(ImGuiMod_Shift | ImGuiKey_Delete);
        ctx->Yield(2);
        IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
        IM_CHECK(fx.ui.deletePermanent);
        ctx->SetRef("//Delete");
        ctx->ItemClick("Delete");
        ctx->Yield(3);
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "beta.md")));
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)3);
    };

    t = IM_REGISTER_TEST(e, "explorer", "copy_paste_into_subfolder");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###alpha.txt");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
        IM_CHECK_EQ(fx.state.clipboard.paths.size(), (size_t)1);
        ctx->ItemDoubleClick("**/###sub");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(app::JoinPath(fx.root, "sub"), "alpha.txt")));
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)2);
    };

    t = IM_REGISTER_TEST(e, "explorer", "cut_paste_moves_and_empties_clipboard");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###beta.md");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_X);
        ctx->Yield();
        IM_CHECK(fx.state.clipboard.cut);
        IM_CHECK(platform::ClipboardHasFiles());
        ctx->ItemDoubleClick("**/###sub");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(app::JoinPath(fx.root, "sub"), "beta.md")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "beta.md")));
        IM_CHECK(!platform::ClipboardHasFiles());
        IM_CHECK(!app::CanPaste(fx.state, fx.state.active()));
    };

    t = IM_REGISTER_TEST(e, "explorer", "drag_row_onto_folder_moves");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        // Step by step, so a failure says which stage broke.
        ctx->MouseMove("**/###alpha.txt");
        ctx->MouseDown(0);
        ctx->MouseLiftDragThreshold();
        ctx->MouseMove("**/###sub", ImGuiTestOpFlags_NoCheckHoveredId);
        ctx->Yield(2);
        IM_CHECK(ImGui::IsDragDropActive());
        IM_CHECK_EQ(fx.ui.dragPaths.size(), (size_t)1);
        IM_CHECK_STR_EQ(fx.ui.dropHoverPath.c_str(), app::JoinPath(fx.root, "sub").c_str());
        ctx->MouseUp(0);
        ctx->Yield(3);
        IM_CHECK_STR_EQ(fx.ui.dialogMessage.c_str(), "");
        IM_CHECK(ImGui::GetTopMostPopupModal() == nullptr);
        IM_CHECK(!ImGui::IsDragDropActive());
        IM_CHECK(platform::PathExists(app::JoinPath(app::JoinPath(fx.root, "sub"), "alpha.txt")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "alpha.txt")));
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)3);
        IM_CHECK(fx.ui.dragPaths.empty());
    };

    t = IM_REGISTER_TEST(e, "explorer", "ctrl_drag_copies_selection");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        // Two selected rows dragged together: both are copied.
        ctx->ItemClick("**/###alpha.txt");
        ctx->KeyDown(ImGuiMod_Ctrl);
        ctx->ItemClick("**/###gamma.png");
        ctx->ItemDragAndDrop("**/###alpha.txt", "**/###sub");
        ctx->KeyUp(ImGuiMod_Ctrl);
        ctx->Yield(3);
        const std::string sub = app::JoinPath(fx.root, "sub");
        IM_CHECK(platform::PathExists(app::JoinPath(sub, "alpha.txt")));
        IM_CHECK(platform::PathExists(app::JoinPath(sub, "gamma.png")));
        IM_CHECK(platform::PathExists(app::JoinPath(fx.root, "alpha.txt")));
        IM_CHECK(platform::PathExists(app::JoinPath(fx.root, "gamma.png")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "drag_onto_breadcrumb");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        const std::string sub = app::JoinPath(fx.root, "sub");
        RefActiveView(ctx, fx);
        ctx->ItemDoubleClick("**/###sub");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());
        // From inside sub, onto the parent's breadcrumb: up it goes.
        const size_t n = app::Breadcrumbs(sub).size();
        ctx->ItemDragAndDrop("**/###inner.txt", ("**/###crumb" + std::to_string(n - 2)).c_str());
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(fx.root, "inner.txt")));
        IM_CHECK(!platform::PathExists(app::JoinPath(sub, "inner.txt")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "drag_onto_nav_pane_folder");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        // The tree shows the scratch folder's own subfolder once the path
        // to it is expanded; navigate there so the tree opens that branch,
        // then drag from the parent tab.
        // %TEMP% sits under the hidden AppData, which the tree skips unless
        // hidden items are shown.
        fx.state.settings.showHidden = true;
        fx.ui.treeChildren.clear();
        const std::string sub = app::JoinPath(fx.root, "sub");
        std::string error;
        app::NavigateTo(fx.state.active(), sub, error);
        ctx->Yield(3);
        app::NavigateTo(fx.state.active(), fx.root, error);
        ctx->Yield(3);
        RefActiveView(ctx, fx);
        ctx->ItemDragAndDrop("**/###gamma.png", "//Navigation/**/###sub");
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(sub, "gamma.png")));
        IM_CHECK(!platform::PathExists(app::JoinPath(fx.root, "gamma.png")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "drop_from_other_program");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        // Stand in for SDL's drop events: a file from outside the folder
        // hovering the "sub" row, then released.
        const std::string outside = app::JoinPath(fx.exeDir, "dropped.txt");
        WriteFile(outside, "from elsewhere");
        RefActiveView(ctx, fx);
        fx.ui.externalDrop = ui::UiState::ExternalDrop{};
        fx.ui.externalDrop.active = true;
        fx.ui.externalDrop.paths.push_back(outside);
        ctx->MouseMove("**/###sub");
        ctx->Yield(2);
        IM_CHECK(fx.ui.externalDrop.haveTarget);
        IM_CHECK_STR_EQ(fx.ui.externalDrop.targetPath.c_str(), app::JoinPath(fx.root, "sub").c_str());
        fx.ui.externalDrop.active = false;
        fx.ui.externalDrop.completed = true;
        fx.ui.externalDrop.ctrl = true;   // copy, so the source stays
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(app::JoinPath(fx.root, "sub"), "dropped.txt")));
        IM_CHECK(platform::PathExists(outside));
        IM_CHECK(!fx.ui.externalDrop.completed);

        // Over nothing in particular: the current folder takes it.
        fx.ui.externalDrop = ui::UiState::ExternalDrop{};
        fx.ui.externalDrop.paths.push_back(outside);
        fx.ui.externalDrop.completed = true;
        fx.ui.externalDrop.ctrl = true;
        ctx->Yield(3);
        IM_CHECK(platform::PathExists(app::JoinPath(fx.root, "dropped.txt")));
    };

    t = IM_REGISTER_TEST(e, "explorer", "command_line_select_scrolls_into_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        // Enough files that the target is well below the fold, then a
        // command-line select of one near the end.
        for (int i = 0; i < 60; ++i) WriteFile(app::JoinPath(fx.root, "row" + std::to_string(i) + ".txt"), "x");
        app::ApplyStartArgument(fx.state, app::JoinPath(fx.root, "row58.txt"));
        ctx->Yield(6);
        const app::Tab& tab = fx.state.active();
        IM_CHECK_EQ(app::SelectedCount(tab), 1);
        IM_CHECK_STR_EQ(tab.entries[(size_t)tab.focused].name.c_str(), "row58.txt");
        // The scroll request was consumed, so SetScrollHereY ran on the row.
        IM_CHECK_EQ(fx.ui.view(tab.id).scrollToEntry, -1);
        // The list is clipped, so a row is registered as an item only while
        // it is drawn: row58 present and a top row (row0) absent means the
        // list really scrolled down to the selection.
        RefActiveView(ctx, fx);
        IM_CHECK(ctx->ItemExists("**/###row58.txt"));
        IM_CHECK(!ctx->ItemExists("**/###row0.txt"));
    };

    t = IM_REGISTER_TEST(e, "explorer", "lasso_rubber_band_selects_rows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        // Sorted: folders first (sub), then alpha.txt, beta.md, gamma.png.
        const ImGuiTestItemInfo alpha = ctx->ItemInfo("**/###alpha.txt");
        const ImGuiTestItemInfo gamma = ctx->ItemInfo("**/###gamma.png");
        IM_CHECK(alpha.ID != 0 && gamma.ID != 0);
        const float x = alpha.RectFull.GetCenter().x;

        // Press in the empty space below the last row and drag up over the
        // three files, not up as far as the sub folder.
        ctx->MouseMoveToPos(ImVec2(x, gamma.RectFull.Max.y + 30.0f));
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(x, gamma.RectFull.GetCenter().y));
        ctx->Yield();
        ctx->MouseMoveToPos(ImVec2(x, alpha.RectFull.GetCenter().y));
        ctx->Yield(2);
        IM_CHECK(fx.ui.view(fx.state.active().id).lassoActive);
        ctx->MouseUp(0);
        ctx->Yield(2);
        const app::Tab& tab = fx.state.active();
        IM_CHECK_EQ(app::SelectedCount(tab), 3);
        for (const char* name : { "alpha.txt", "beta.md", "gamma.png" })
            {
            int idx = -1;
            for (size_t i = 0; i < tab.entries.size(); ++i)
                if (tab.entries[i].name == name) idx = (int)i;
            IM_CHECK(idx >= 0 && tab.selected[(size_t)idx]);
            }

        // Plain lasso replaces: a fresh band over just gamma leaves one.
        ctx->MouseMoveToPos(ImVec2(x, gamma.RectFull.Max.y + 30.0f));
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(x, gamma.RectFull.GetCenter().y));
        ctx->Yield(2);
        ctx->MouseUp(0);
        ctx->Yield(2);
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 1);

        // A plain click in empty space clears the selection.
        ctx->MouseMoveToPos(ImVec2(x, gamma.RectFull.Max.y + 30.0f));
        ctx->MouseClick(0);
        ctx->Yield(2);
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 0);
    };

    t = IM_REGISTER_TEST(e, "explorer", "lasso_from_right_margin");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        RefActiveView(ctx, fx);
        const ImGuiTestItemInfo alpha = ctx->ItemInfo("**/###alpha.txt");
        const ImGuiTestItemInfo gamma = ctx->ItemInfo("**/###gamma.png");
        IM_CHECK(alpha.ID != 0 && gamma.ID != 0);
        // Start in the empty area to the right of the columns (past each
        // row's right edge) and drag straight down over the three files.
        const float mx = gamma.RectFull.Max.x + 16.0f;
        ctx->MouseMoveToPos(ImVec2(mx, alpha.RectFull.Min.y + 2.0f));
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(mx, gamma.RectFull.Max.y - 2.0f));
        ctx->Yield(2);
        IM_CHECK(fx.ui.view(fx.state.active().id).lassoActive);
        ctx->MouseUp(0);
        ctx->Yield(2);
        // The three files, selected by vertical overlap; the sub folder sits
        // above alpha and is left out.
        IM_CHECK_EQ(app::SelectedCount(fx.state.active()), 3);
    };

    t = IM_REGISTER_TEST(e, "explorer", "shortcut_bar_pin_jump_clear");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        for (std::string& s : fx.state.settings.shortcuts) s.clear();
        const std::string sub = app::JoinPath(fx.root, "sub");

        // Select the folder, click an empty slot: the slot takes it.
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###sub");
        ctx->SetRef(ui::kHostWindow);
        ctx->ItemClick("**/###shortcut0");
        ctx->Yield();
        IM_CHECK(app::ShortcutAssigned(fx.state, 0));
        IM_CHECK_STR_EQ(app::ShortcutPath(fx.state, 0).c_str(), sub.c_str());
        IM_CHECK(!app::ShortcutAssigned(fx.state, 1));

        // Nothing selected: an empty slot takes the folder being shown.
        app::ClearSelection(fx.state.active());
        ctx->ItemClick("**/###shortcut1");
        ctx->Yield();
        IM_CHECK_STR_EQ(app::ShortcutPath(fx.state, 1).c_str(), fx.root.c_str());

        // From then on the slot jumps the active view there.
        ctx->ItemClick("**/###shortcut0");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());
        ctx->ItemClick("**/###shortcut1");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), fx.root.c_str());

        // Ctrl+click opens it in a new view.
        ctx->KeyDown(ImGuiMod_Ctrl);
        ctx->ItemClick("**/###shortcut0");
        ctx->KeyUp(ImGuiMod_Ctrl);
        ctx->Yield(3);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());

        // Right-click, Clear: empty again, and the file on disk agrees.
        ctx->ItemClick("**/###shortcut0", ImGuiMouseButton_Right);
        ctx->Yield();
        ctx->ItemClick("//$FOCUSED/Clear");
        ctx->Yield();
        IM_CHECK(!app::ShortcutAssigned(fx.state, 0));
        app::Settings onDisk;
        IM_CHECK(app::LoadSettings(fx.state.settingsFile, onDisk));
        IM_CHECK(onDisk.shortcuts[0].empty());
        IM_CHECK_STR_EQ(onDisk.shortcuts[1].c_str(), fx.root.c_str());
        for (std::string& s : fx.state.settings.shortcuts) s.clear();
    };

    t = IM_REGISTER_TEST(e, "explorer", "nav_pane_click");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        ctx->SetRef(ui::kNavWindow);
        ctx->ItemClick("**/###ThisPC");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), "");
        std::string desktop;
        for (const platform::KnownFolder& kf : fx.state.quickAccess)
            if (kf.kind == platform::KnownFolderKind::Desktop) desktop = kf.path;
        IM_CHECK(!desktop.empty());
        ctx->ItemClick("**/###Desktop");
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), desktop.c_str());
    };

    t = IM_REGISTER_TEST(e, "explorer", "hidden_items_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
#ifdef _WIN32
        const std::string hidden = app::JoinPath(fx.root, "secret.txt");
        WriteFile(hidden, "shh");
        SetFileAttributesW(platform::Utf8ToWide(hidden).c_str(), FILE_ATTRIBUTE_HIDDEN);
#else
        WriteFile(app::JoinPath(fx.root, ".secret.txt"), "shh");
#endif
        RefActiveView(ctx, fx);
        ctx->ItemClick("**/###Refresh");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.active().visible.size(), (size_t)4);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("View/Hidden items");
        ctx->Yield(2);
        IM_CHECK(fx.state.settings.showHidden);
        IM_CHECK_EQ(fx.state.active().visible.size(), (size_t)5);
        ctx->MenuClick("View/Hidden items");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.active().visible.size(), (size_t)4);
    };
}

} // namespace

// Usage: davesplorer_e2e_tests [filter] [-v]
//   filter  runs only tests whose name matches (test-engine filter syntax)
//   -v      debug-level log for each test
int main(int argc, char** argv)
{
#ifndef _WIN32
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
#endif
    const char* filter = nullptr;
    bool verbose = false;
    for (int i = 1; i < argc; ++i)
        {
        if (std::string(argv[i]) == "-v") verbose = true;
        else filter = argv[i];
        }

    Fixture fx;
    g_fx = &fx;
    fx.root = TempRoot("e2e");
    fx.exeDir = TempRoot("e2e_home");
    std::string error;
    platform::CreateFolder(fx.exeDir, error);
    ResetScratch(fx);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1400, 900);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = 0;
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = 0;

    std::string fontError;
    ui::fonts::Load(DSP_ASSETS_DIR, 18.0f, fontError);
    if (!fontError.empty()) std::printf("note: %s\n", fontError.c_str());

    app::InitAppState(fx.state, fx.exeDir);
    fx.state.buildInfo = "e2e harness";
    ui::ApplyAppearanceNow(fx.state.settings.themeIndex, 1.0f, 1.0f);

    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& teio = ImGuiTestEngine_GetIO(engine);
    teio.ConfigVerboseLevel = verbose ? ImGuiTestVerboseLevel_Debug : ImGuiTestVerboseLevel_Warning;
    teio.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    teio.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    teio.ConfigNoThrottle = true;
    teio.ConfigLogToTTY = true;
    teio.CoroutineFuncs = Coroutine_ImplStdThread_GetInterface();
    RegisterTests(engine);
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    ImGuiTestEngine_InstallDefaultCrashHandler();
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter, ImGuiTestRunFlags_RunFromCommandLine);

    int frames = 0;
    while (!ImGuiTestEngine_IsTestQueueEmpty(engine) && frames < 60 * 60 * 5)
        {
        ui::PumpAppearance(fx.state.settings.themeIndex, fx.state.settings.uiScale, 1.0f);
        app::TickAppState(fx.state);
        ImGui::NewFrame();
        ui::DrawFrame(fx.state, fx.ui);
        ImGui::Render();
        AcknowledgeTextures();
        ImGuiTestEngine_PostSwap(engine);
        ++frames;
        }

    int tested = 0, passed = 0;
    ImGuiTestEngine_GetResult(engine, tested, passed);
    ImVector<ImGuiTest*> tests;
    ImGuiTestEngine_GetTestList(engine, &tests);
    for (ImGuiTest* test : tests)
        {
        const char* status = test->Output.Status == ImGuiTestStatus_Success ? "ok"
                           : test->Output.Status == ImGuiTestStatus_Unknown ? "skipped" : "FAILED";
        std::printf("%-8s %s/%s\n", status, test->Category, test->Name);
        }

    ImGuiTestEngine_Stop(engine);
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);

    platform::DeletePermanently({ fx.root, fx.exeDir }, error);
    std::printf("%d of %d e2e tests passed in %d frames\n", passed, tested, frames);
    return (tested > 0 && passed == tested) ? 0 : 1;
}
