#include "platform/Host.h"
#include "platform/DragDrop.h"
#include "platform/Paths.h"
#include "platform/Strings.h"

#include "app/AppState.h"
#include "ui/Fonts.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/UiState.h"

#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <glib.h>
#include <filesystem>
#endif
#include "app/PathUtil.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <cstdio>
#include <string>

namespace platform
{

namespace
{

constexpr float kBaseFontPx = 18.0f;

std::string BuildInfo()
{
    const int v = SDL_GetVersion();
    char buf[128];
    std::snprintf(buf, sizeof(buf), "SDL %d.%d.%d, built %s", SDL_VERSIONNUM_MAJOR(v),
                  SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v), __DATE__);
    return buf;
}

void ShowFatal(const char* what)
{
    // Stderr goes nowhere for a WIN32-subsystem exe; the message box is the
    // only channel that reaches the user.
    std::fprintf(stderr, "%s\n", what);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Davesplorer", what, nullptr);
}

} // namespace

int RunApplication(int argc, char** argv)
{
    SDL_SetAppMetadata("Davesplorer", "0.1.0", "com.davesplorer.Davesplorer");
    // The window class takes its icon from resource 101 in app.rc, which
    // is the same red folder Explorer shows on the exe; no pixel data has
    // to be shipped or decoded for it.
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "101");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "101");
    if (!SDL_Init(SDL_INIT_VIDEO))
        {
        ShowFatal(SDL_GetError());
        return 1;
        }

    app::AppState state;
#ifdef _WIN32
    app::InitAppState(state, ExecutableDir());
#else
    const std::string configDir = app::JoinPath(g_get_user_config_dir(), "davesplorer");
    std::error_code configError;
    std::filesystem::create_directories(configDir, configError);
    if (configError)
        {
        ShowFatal(("Could not create settings directory: " + configError.message()).c_str());
        SDL_Quit();
        return 1;
        }
    app::InitAppState(state, ExecutableDir(), configDir);
#endif
    // "davesplorer.exe <folder-or-file>": what the shell passes when a
    // folder is opened with this program. SDL's main shim already
    // converted the arguments to UTF-8.
    if (argc > 1 && argv[1] && argv[1][0] != '\0') app::ApplyStartArgument(state, argv[1]);
    state.buildInfo = BuildInfo();
    const app::Settings& s = state.settings;

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (s.windowMaximized) flags |= SDL_WINDOW_MAXIMIZED;
    SDL_Window* window = SDL_CreateWindow("Davesplorer", s.windowWidth, s.windowHeight, flags);
    if (!window)
        {
        ShowFatal(SDL_GetError());
        SDL_Quit();
        return 1;
        }
#ifndef _WIN32
    const std::string iconPath = app::JoinPath(state.exeDir, "assets/icon/davesplorer.png");
    if (SDL_Surface* icon = SDL_LoadPNG(iconPath.c_str()))
        {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
        }
#endif
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
        {
        ShowFatal(SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
        }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = state.layoutFile.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDpiScaleFonts = true;
    // Explorer's Ctrl+Tab switches tabs; ImGui's default would open its
    // window-switching overlay on the same chord.
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = 0;
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = 0;

    std::string fontError;
    ui::fonts::Load(app::JoinPath(state.exeDir, "assets"), kBaseFontPx, fontError);

    ui::UiState uiState;
    uiState.dpiScale = SDL_GetWindowDisplayScale(window);
    ui::ApplyAppearanceNow(s.themeIndex, s.uiScale, uiState.dpiScale);
    if (!fontError.empty()) state.statusMessage = fontError;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    SDL_ShowWindow(window);

    bool running = true;
    while (running && !state.quitRequested)
        {
        SDL_Event event;
        // Block until something happens rather than spinning: a file
        // explorer sitting idle should cost nothing. The timeout keeps
        // tooltips and blinking cursors alive.
        if (SDL_WaitEventTimeout(&event, 100))
            {
            do
                {
                ImGui_ImplSDL3_ProcessEvent(&event);
                switch (event.type)
                    {
                    case SDL_EVENT_QUIT:
                        running = false;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        if (event.window.windowID == SDL_GetWindowID(window)) running = false;
                        break;
                    // A drag from another program. OLE owns the mouse while
                    // it lasts, so the drop position is fed to ImGui by hand
                    // for the panes to find the folder under the cursor.
                    case SDL_EVENT_DROP_BEGIN:
                        uiState.externalDrop = ui::UiState::ExternalDrop{};
                        uiState.externalDrop.active = true;
                        break;
                    case SDL_EVENT_DROP_POSITION:
                        io.AddMousePosEvent(event.drop.x, event.drop.y);
                        break;
                    case SDL_EVENT_DROP_FILE:
                        io.AddMousePosEvent(event.drop.x, event.drop.y);
                        if (event.drop.data) uiState.externalDrop.paths.push_back(event.drop.data);
                        break;
                    case SDL_EVENT_DROP_COMPLETE:
                        uiState.externalDrop.active = false;
                        uiState.externalDrop.completed = true;
#ifdef _WIN32
                        uiState.externalDrop.ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        uiState.externalDrop.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#else
                        uiState.externalDrop.ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
                        uiState.externalDrop.shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
#endif
                        break;
                    default:
                        break;
                    }
                }
            while (SDL_PollEvent(&event));
            }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
            {
            SDL_Delay(50);
            continue;
            }

        uiState.dpiScale = SDL_GetWindowDisplayScale(window);
        // Between frames only: ScaleAllSizes is not idempotent and a
        // mid-frame metric change leaves the rest of the frame inconsistent.
        ui::PumpAppearance(state.settings.themeIndex, state.settings.uiScale, uiState.dpiScale);

        app::TickAppState(state);

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ui::DrawFrame(state, uiState);
        ImGui::Render();

        const ImVec4& bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, bg.x, bg.y, bg.z, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // An in-app drag crossed the window edge: hand it to OLE so Explorer
        // and other programs can take it. DoDragDrop blocks until the drop;
        // the button-up happens while OLE has the mouse, so ImGui is told
        // about it afterwards or it would keep dragging.
        if (uiState.externalDragRequested)
            {
            uiState.externalDragRequested = false;
            const std::vector<std::string> paths = uiState.dragPaths;
            uiState.dragPaths.clear();
            ImGui::ClearDragDrop();
            io.AddMouseButtonEvent(0, false);
            DragOutcome outcome;
            std::string error;
            if (!DragFilesOut(paths, outcome, error)) state.statusMessage = error;
            app::RefreshAll(state);
            }
        }

    // Window geometry goes back into settings so the next launch opens the
    // same way.
    const SDL_WindowFlags finalFlags = SDL_GetWindowFlags(window);
    state.settings.windowMaximized = (finalFlags & SDL_WINDOW_MAXIMIZED) != 0;
    if (!state.settings.windowMaximized)
        SDL_GetWindowSize(window, &state.settings.windowWidth, &state.settings.windowHeight);
    app::SaveAppState(state);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace platform
