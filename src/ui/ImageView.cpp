// The built-in image viewer: a file view is a dockable window showing one
// image, with zoom and pan, and Previous/Next through the folder's other
// images. It sits beside the folder views as a tab, floats, or docks
// anywhere they can.
#include "ui/MainWindow.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Textures.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/Format.h"
#include "app/Image.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "platform/FileSystem.h"
#include "platform/Shell.h"
#include "platform/Strings.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace ui
{

std::string FileViewWindowName(const app::AppState&, const app::FileView& view)
{
    return app::PathName(view.path) + "###file" + std::to_string(view.id);
}

int OpenFileView(app::AppState& state, UiState& ui, const std::string& path)
{
    // Beside the active folder view, as a tab, the way a new folder view
    // lands; a view already showing the file is simply brought forward.
    ImGuiID target = 0;
    if (!state.tabs.empty()) target = ui.view(state.active().id).dockId;
    if (target == 0) target = ui.defaultViewDock;

    const bool existed = app::FindFileView(state, path) >= 0;
    const int index = app::OpenFileView(state, path);
    FileViewUi& v = ui.fileView(state.fileViews[(size_t)index].id);
    v.wantFocus = true;
    if (!existed) v.dockHint = target;
    return index;
}

void CloseFileView(app::AppState& state, UiState& ui, int id)
{
    auto it = ui.fileViews.find(id);
    if (it != ui.fileViews.end())
        {
        textures::Release(it->second.texture);
        ui.fileViews.erase(it);
        }
    if (ui.activeFileView == id) ui.activeFileView = -1;
    for (int i = 0; i < (int)state.fileViews.size(); ++i)
        if (state.fileViews[(size_t)i].id == id)
            {
            app::CloseFileView(state, i);
            break;
            }
}

namespace
{

// One GPU texture per image; a larger photo is reduced on load. 8192 is
// within every renderer's limit and keeps the worst case at 256 MB.
constexpr int   kMaxTextureDim = 8192;
constexpr float kZoomMin = 0.02f;
constexpr float kZoomMax = 32.0f;
constexpr float kZoomStep = 1.25f;

bool SamePath(const std::string& a, const std::string& b)
{
#ifdef _WIN32
    return platform::ToLowerAscii(a) == platform::ToLowerAscii(b);
#else
    return a == b;
#endif
}

uint64_t FileBytes(const std::string& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())), ec);
    return ec ? 0 : (uint64_t)size;
}

// Decodes the view's file the first time it is drawn, and again whenever
// Previous/Next changed the path. The folder's image list is re-read at
// the same time, so a file added or removed since is noticed.
void LoadIfNeeded(app::FileView& view, FileViewUi& v)
{
    if (v.loadedPath == view.path) return;
    textures::Release(v.texture);
    v.texture = nullptr;
    v.width = v.height = v.sourceWidth = v.sourceHeight = 0;
    v.hasAlpha = false;
    v.fileBytes = 0;
    v.error.clear();
    v.fit = true;
    v.zoom = 1.0f;
    v.pan = ImVec2();
    v.loadedPath = view.path;

    app::DecodedImage image;
    std::string error;
    if (app::DecodeImageFile(view.path, kMaxTextureDim, image, error))
        {
        v.texture = textures::Create(image.width, image.height, image.rgba.data());
        v.width = image.width;
        v.height = image.height;
        v.sourceWidth = image.sourceWidth;
        v.sourceHeight = image.sourceHeight;
        v.hasAlpha = image.hasAlpha;
        }
    else
        {
        v.error = error;
        }
    v.fileBytes = FileBytes(view.path);

    v.siblings = app::ImageSiblings(view.path);
    v.siblingIndex = -1;
    for (size_t i = 0; i < v.siblings.size(); ++i)
        if (SamePath(v.siblings[i], view.path)) v.siblingIndex = (int)i;
}

// Previous/Next/First/Last. The path changes now; the image loads on the
// next draw.
void StepImage(app::FileView& view, FileViewUi& v, int delta, bool toEnd)
{
    const int n = (int)v.siblings.size();
    if (n == 0) return;
    int target = toEnd ? (delta < 0 ? 0 : n - 1) : v.siblingIndex + delta;
    if (v.siblingIndex < 0) target = 0;
    target = std::clamp(target, 0, n - 1);
    if (target == v.siblingIndex) return;
    view.path = v.siblings[(size_t)target];
}

// Zoom so the image point under `anchor` stays put: the natural behaviour
// for the wheel and for double-click.
void ZoomAt(FileViewUi& v, float newZoom, ImVec2 anchor, ImVec2 canvasCenter)
{
    newZoom = std::clamp(newZoom, kZoomMin, kZoomMax);
    const ImVec2 center(canvasCenter.x + v.pan.x, canvasCenter.y + v.pan.y);
    const float ratio = newZoom / v.zoom;
    const ImVec2 newCenter(anchor.x - (anchor.x - center.x) * ratio, anchor.y - (anchor.y - center.y) * ratio);
    v.pan = ImVec2(newCenter.x - canvasCenter.x, newCenter.y - canvasCenter.y);
    v.zoom = newZoom;
    v.fit = false;
}

// An image smaller than the canvas sits centred; a larger one may be
// panned, but never past its own edge.
void ClampPan(FileViewUi& v, ImVec2 shown, ImVec2 avail)
{
    const float lx = std::max(0.0f, (shown.x - avail.x) * 0.5f);
    const float ly = std::max(0.0f, (shown.y - avail.y) * 0.5f);
    v.pan.x = std::clamp(v.pan.x, -lx, lx);
    v.pan.y = std::clamp(v.pan.y, -ly, ly);
}

// The transparent-background checkerboard image editors show, drawn only
// over the visible part of the image so a deep zoom costs nothing extra.
void DrawCheckerboard(ImDrawList* dl, ImVec2 imgMin, ImVec2 imgMax, ImRect clip, float cell)
{
    const bool dark = ThemeAt(CurrentThemeIndex()).isDark;
    const ImU32 a = Lift(CurrentPalette().surface, 0.06f, dark);
    const ImU32 b = Lift(CurrentPalette().surface, 0.16f, dark);
    ImRect r(imgMin, imgMax);
    r.ClipWith(clip);
    if (r.Min.x >= r.Max.x || r.Min.y >= r.Max.y) return;
    dl->AddRectFilled(r.Min, r.Max, a);
    const int cx0 = (int)std::floor((r.Min.x - imgMin.x) / cell);
    const int cy0 = (int)std::floor((r.Min.y - imgMin.y) / cell);
    const int cx1 = (int)std::ceil((r.Max.x - imgMin.x) / cell);
    const int cy1 = (int)std::ceil((r.Max.y - imgMin.y) / cell);
    if ((long long)(cx1 - cx0) * (cy1 - cy0) > 40000) return;   // absurdly small cells: the flat fill will do
    for (int cy = cy0; cy < cy1; ++cy)
        for (int cx = cx0; cx < cx1; ++cx)
            {
            if (((cx + cy) & 1) == 0) continue;
            const ImVec2 p0(std::max(r.Min.x, imgMin.x + cx * cell), std::max(r.Min.y, imgMin.y + cy * cell));
            const ImVec2 p1(std::min(r.Max.x, imgMin.x + (cx + 1) * cell), std::min(r.Max.y, imgMin.y + (cy + 1) * cell));
            if (p0.x < p1.x && p0.y < p1.y) dl->AddRectFilled(p0, p1, b);
            }
}

// Brings the active folder view to the file: navigates it to the folder
// and selects the file, the way "Show in folder" does in a browser.
void ShowInFolderView(app::AppState& state, UiState& ui, const std::string& path)
{
    if (state.tabs.empty()) return;
    app::Tab& tab = state.active();
    const std::string folder = app::ParentPath(path);
    std::string error;
    if (!SamePath(tab.path, folder) && !app::NavigateTo(tab, folder, error))
        {
        ShowError(ui, error);
        return;
        }
    app::ClearSelection(tab);
    tab.selectOnLoad = app::PathName(path);
    app::Refresh(tab);
    ui.view(tab.id).wantFocus = true;
}

void DrawImageToolbar(app::AppState& state, UiState& ui, app::FileView& view, FileViewUi& v, ImVec2 canvasCenter)
{
    const int n = (int)v.siblings.size();
    if (IconButton("Prev", ICON_MD_NAVIGATE_BEFORE, "Previous image (Left)", v.siblingIndex > 0)) StepImage(view, v, -1, false);
    ImGui::SameLine();
    if (IconButton("Next", ICON_MD_NAVIGATE_NEXT, "Next image (Right)", v.siblingIndex >= 0 && v.siblingIndex + 1 < n)) StepImage(view, v, 1, false);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    const bool haveImage = v.texture != nullptr;
    if (IconButton("ZoomOut", ICON_MD_ZOOM_OUT, "Zoom out (-)", haveImage)) ZoomAt(v, v.zoom / kZoomStep, canvasCenter, canvasCenter);
    ImGui::SameLine();
    // Percent of the file's real pixels, even when the texture was reduced.
    const float shownZoom = (v.sourceWidth > 0) ? v.zoom * (float)v.width / (float)v.sourceWidth : v.zoom;
    char pct[32];
    std::snprintf(pct, sizeof(pct), "%d%%", (int)std::lround(shownZoom * 100.0f));
    // Centred in a fixed slot so the buttons either side do not shuffle as
    // the number changes width.
    const float slotW = ImGui::CalcTextSize("1000%").x;
    const float slotX = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(slotX + std::max(0.0f, (slotW - ImGui::CalcTextSize(pct).x) * 0.5f));
    ImGui::AlignTextToFramePadding();
    MutedText(haveImage ? pct : "");
    ImGui::SameLine();
    ImGui::SetCursorPosX(slotX + slotW + ImGui::GetStyle().ItemSpacing.x);
    if (IconButton("ZoomIn", ICON_MD_ZOOM_IN, "Zoom in (+)", haveImage)) ZoomAt(v, v.zoom * kZoomStep, canvasCenter, canvasCenter);
    ImGui::SameLine();
    if (IconButton("Fit", ICON_MD_FIT_SCREEN, "Fit to window (0)", haveImage)) v.fit = true;
    ImGui::SameLine();
    if (IconButton("ActualSize", ICON_MD_CROP_FREE, "Actual size (1)", haveImage))
        ZoomAt(v, (v.sourceWidth > 0) ? (float)v.sourceWidth / (float)v.width : 1.0f, canvasCenter, canvasCenter);

    // Right-hand cluster: the file's folder view, and the program the
    // shell would have used.
    const ImGuiStyle& style = ImGui::GetStyle();
    auto buttonW = [&](const char* icon) { return ImGui::CalcTextSize(icon).x + style.FramePadding.x * 2.0f; };
    const float cluster = buttonW(ICON_MD_FOLDER_OPEN) + buttonW(ICON_MD_OPEN_IN_NEW) + style.ItemSpacing.x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - cluster));
    if (IconButton("ShowInFolder", ICON_MD_FOLDER_OPEN, "Show in folder view")) ShowInFolderView(state, ui, view.path);
    ImGui::SameLine();
    if (IconButton("OpenExternal", ICON_MD_OPEN_IN_NEW, "Open with default program"))
        {
        std::string error;
        if (!platform::OpenWithShell(view.path, error)) ShowError(ui, error);
        }
}

void DrawImageCanvas(FileViewUi& v, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CurrentPalette().surface);
    ImGui::BeginChild("##canvas", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 avail(std::max(ImGui::GetContentRegionAvail().x, 1.0f), std::max(ImGui::GetContentRegionAvail().y, 1.0f));
    const ImVec2 p1(p0.x + avail.x, p0.y + avail.y);
    const ImVec2 canvasCenter((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
    ImGuiIO& io = ImGui::GetIO();

    // One item covering the canvas: it takes the focus on click, reports
    // hover for the wheel, and is what the drag-to-pan holds.
    ImGui::InvisibleButton("###canvas", avail, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    if (v.texture)
        {
        const float w = (float)v.width, h = (float)v.height;
        if (v.fit)
            {
            // Shrink to fit, never enlarge: a small icon stays its size.
            v.zoom = std::min(1.0f, std::min(avail.x / w, avail.y / h));
            v.pan = ImVec2();
            }
        if (hovered && io.MouseWheel != 0.0f && !io.KeyCtrl)
            ZoomAt(v, v.zoom * std::pow(kZoomStep, io.MouseWheel), io.MousePos, canvasCenter);
        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
            // Fit <-> real pixels, about the point clicked.
            if (v.fit) ZoomAt(v, (v.sourceWidth > 0) ? (float)v.sourceWidth / w : 1.0f, io.MousePos, canvasCenter);
            else v.fit = true;
            }
        if (held && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
            {
            v.pan.x += io.MouseDelta.x;
            v.pan.y += io.MouseDelta.y;
            }
        const ImVec2 shown(w * v.zoom, h * v.zoom);
        ClampPan(v, shown, avail);

        // Whole pixels for the top-left corner: at 100% the image lands
        // on the pixel grid and stays crisp.
        const ImVec2 imgMin(std::floor(canvasCenter.x + v.pan.x - shown.x * 0.5f), std::floor(canvasCenter.y + v.pan.y - shown.y * 0.5f));
        const ImVec2 imgMax(imgMin.x + shown.x, imgMin.y + shown.y);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(p0, p1, true);
        if (v.hasAlpha)
            DrawCheckerboard(dl, imgMin, imgMax, ImRect(p0, p1), 8.0f * ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi);
        dl->AddImage(v.texture->GetTexRef(), imgMin, imgMax);
        dl->PopClipRect();
        if (hovered && shown.x > avail.x + 0.5f) ImGui::SetMouseCursor(held ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);
        else if (hovered && shown.y > avail.y + 0.5f) ImGui::SetMouseCursor(held ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);
        }
    else if (!v.error.empty())
        {
        const ImVec2 textSize = ImGui::CalcTextSize(v.error.c_str(), nullptr, false, avail.x - 40.0f);
        ImGui::SetCursorScreenPos(ImVec2(canvasCenter.x - textSize.x * 0.5f, canvasCenter.y - textSize.y * 0.5f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textSize.x);
        ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().danger);
        ImGui::TextUnformatted(v.error.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        }
    ImGui::EndChild();
}

void DrawImageStatusBar(const app::FileView& view, const FileViewUi& v)
{
    std::string text;
    if (v.sourceWidth > 0)
        {
        text = std::to_string(v.sourceWidth) + " \xc3\x97 " + std::to_string(v.sourceHeight);
        if (v.sourceWidth != v.width) text += " (shown reduced)";
        }
    const std::string ext = app::PathName(view.path);
    const size_t dot = ext.find_last_of('.');
    const std::string typeName = platform::FileTypeName(dot == std::string::npos ? "" : platform::ToLowerAscii(ext.substr(dot + 1)), false);
    if (!typeName.empty()) text += (text.empty() ? "" : "    ") + typeName;
    if (v.fileBytes > 0) text += "    " + app::HumanSize(v.fileBytes);
    if (v.siblingIndex >= 0) text += "    " + std::to_string(v.siblingIndex + 1) + " of " + std::to_string(v.siblings.size());
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    MutedText(text.c_str());
}

// Keys while this window has focus: stepping, zooming. Plain keys only;
// the Ctrl chords belong to the application.
void HandleImageKeys(app::FileView& view, FileViewUi& v, ImVec2 canvasCenter)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || io.KeyCtrl) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) return;

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_PageUp))    StepImage(view, v, -1, false);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_PageDown)) StepImage(view, v, 1, false);
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) StepImage(view, v, -1, true);
    if (ImGui::IsKeyPressed(ImGuiKey_End))  StepImage(view, v, 1, true);
    if (!v.texture) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))      ZoomAt(v, v.zoom * kZoomStep, canvasCenter, canvasCenter);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) ZoomAt(v, v.zoom / kZoomStep, canvasCenter, canvasCenter);
    if (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0)) v.fit = true;
    if (ImGui::IsKeyPressed(ImGuiKey_1) || ImGui::IsKeyPressed(ImGuiKey_Keypad1))
        ZoomAt(v, (v.sourceWidth > 0) ? (float)v.sourceWidth / (float)v.width : 1.0f, canvasCenter, canvasCenter);
}

} // namespace

bool DrawFileView(app::AppState& state, UiState& ui, int index)
{
    app::FileView& view = state.fileViews[(size_t)index];
    FileViewUi& v = ui.fileView(view.id);

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
        ImGui::SetNextWindowDockID(ui.defaultViewDock, ImGuiCond_FirstUseEver);
        }
    v.shown = true;

    const std::string name = FileViewWindowName(state, view);
    bool open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    const bool visible = ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (visible)
        {
        v.dockId = ImGui::GetWindowDockID();
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) ui.activeFileView = view.id;
        LoadIfNeeded(view, v);

        // The canvas centre is what keyboard and toolbar zooms pivot on;
        // it is known from the layout before the canvas is drawn.
        const float statusHeight = state.settings.showStatusBar
                                       ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 2
                                       : 0.0f;
        const float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize(avail.x, std::max(avail.y - statusHeight - toolbarHeight, 1.0f));
        const ImVec2 canvasCenter(origin.x + canvasSize.x * 0.5f, origin.y + toolbarHeight + canvasSize.y * 0.5f);

        DrawImageToolbar(state, ui, view, v, canvasCenter);
        DrawImageCanvas(v, canvasSize);
        if (state.settings.showStatusBar) DrawImageStatusBar(view, v);
        HandleImageKeys(view, v, canvasCenter);
        }
    ImGui::End();
    return !open;
}

} // namespace ui
