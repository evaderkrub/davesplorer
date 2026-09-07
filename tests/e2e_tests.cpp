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

#include <windows.h>
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
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    return platform::WideToUtf8(tmp) + "davesplorer_" + tag + "_" + std::to_string(GetCurrentProcessId());
}

void WriteFile(const std::string& path, const char* content)
{
    std::ofstream(platform::Utf8ToWide(path)) << content;
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

// Every test starts from one tab in the scratch folder.
void GoToScratch(ImGuiTestContext* ctx, Fixture& fx)
{
    ResetScratch(fx);
    fx.state.tabs.clear();
    fx.state.activeTab = 0;
    app::OpenTab(fx.state, fx.root);
    fx.state.settings.uiScale = 1.0f;
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
        IM_CHECK(ctx->WindowInfo(ui::kFilesWindow).Window != nullptr);
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
        ctx->SetRef(ui::kFilesWindow);
        ctx->ItemClick("**/###addressclick");
        ctx->Yield();
        IM_CHECK(fx.ui.addressEditing);
        const std::string sub = app::JoinPath(fx.root, "sub");
        ctx->ItemInputValue("**/##address", sub.c_str());
        ctx->Yield(2);
        IM_CHECK_STR_EQ(fx.state.active().path.c_str(), sub.c_str());
        IM_CHECK_EQ(fx.state.active().entries.size(), (size_t)1);
        IM_CHECK(!fx.ui.addressEditing);

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
        ctx->SetRef(ui::kFilesWindow);
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
        ctx->SetRef(ui::kFilesWindow);
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
        ctx->SetRef(ui::kFilesWindow);
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
        ctx->SetRef(ui::kFilesWindow);
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

    t = IM_REGISTER_TEST(e, "explorer", "tabs_open_close");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        Fixture& fx = *g_fx;
        GoToScratch(ctx, fx);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_T);
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        IM_CHECK_EQ(fx.state.activeTab, 1);
        ctx->SetRef(ui::kFilesWindow);
        ctx->ItemClick("**/###NewTab");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)3);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_W);
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)2);
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("File/Close tab");
        ctx->Yield(2);
        IM_CHECK_EQ(fx.state.tabs.size(), (size_t)1);
        IM_CHECK(!fx.state.quitRequested);
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

        // An invalid name keeps the dialog open with a reason.
        ctx->SetRef(ui::kHostWindow);
        ctx->MenuClick("File/New folder");
        ctx->Yield(2);
        ctx->SetRef("//New folder");
        ctx->ItemInputValue("##name", "bad<name");
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
        ctx->SetRef(ui::kFilesWindow);
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
        ctx->SetRef(ui::kFilesWindow);
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
        ctx->SetRef(ui::kFilesWindow);
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
        const std::string hidden = app::JoinPath(fx.root, "secret.txt");
        WriteFile(hidden, "shh");
        SetFileAttributesW(platform::Utf8ToWide(hidden).c_str(), FILE_ATTRIBUTE_HIDDEN);
        ctx->SetRef(ui::kFilesWindow);
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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

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
    teio.ConfigVerboseLevel = ImGuiTestVerboseLevel_Warning;
    teio.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    teio.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    teio.ConfigNoThrottle = true;
    teio.ConfigLogToTTY = true;
    teio.CoroutineFuncs = Coroutine_ImplStdThread_GetInterface();
    RegisterTests(engine);
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    ImGuiTestEngine_InstallDefaultCrashHandler();
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, nullptr, ImGuiTestRunFlags_RunFromCommandLine);

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
        std::printf("%-8s %s/%s\n", test->Output.Status == ImGuiTestStatus_Success ? "ok" : "FAILED",
                    test->Category, test->Name);

    ImGuiTestEngine_Stop(engine);
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);

    platform::DeletePermanently({ fx.root, fx.exeDir }, error);
    std::printf("%d of %d e2e tests passed in %d frames\n", passed, tested, frames);
    return (tested > 0 && passed == tested) ? 0 : 1;
}
