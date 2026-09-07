#include "ui/DragDrop.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include "app/FileOps.h"
#include "app/PathUtil.h"
#include "app/Selection.h"

#include "imgui_internal.h"

namespace ui
{

namespace
{

constexpr const char* kPayload = "DSP_FILES";

void HighlightRect(const ImVec2& mn, const ImVec2& mx)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(mn, mx, WithAlpha(CurrentPalette().accent, 40), ImGui::GetStyle().FrameRounding);
    dl->AddRect(mn, mx, CurrentPalette().accent, ImGui::GetStyle().FrameRounding, 0, 2.0f);
}

// Shared by the item and the rectangle variants once the hit test is done.
// `delivered` must be read from the payload BEFORE EndDragDropTarget(): on
// the delivery frame that call clears the payload, flag included.
bool HandleTargetHit(app::AppState& state, UiState& ui, const std::string& folderPath, const ImVec2& mn, const ImVec2& mx,
                     bool inAppHit, bool delivered, bool externalHit)
{
    bool hovered = false;
    if (inAppHit && app::CanDropOn(ui.dragPaths, folderPath))
        {
        hovered = true;
        ui.dropHoverPathNext = folderPath;
        HighlightRect(mn, mx);
        if (delivered)
            {
            const ImGuiIO& io = ImGui::GetIO();
            PerformDrop(state, ui, ui.dragPaths, folderPath, io.KeyCtrl, io.KeyShift);
            ui.dragPaths.clear();
            }
        }
    if (externalHit && !folderPath.empty())
        {
        hovered = true;
        ui.externalDrop.targetPath = folderPath;
        ui.externalDrop.haveTarget = true;
        HighlightRect(mn, mx);
        }
    return hovered;
}

} // namespace

void BeginDragDropFrame(app::AppState& state, UiState& ui)
{
    ui.dropHoverPath = ui.dropHoverPathNext;
    ui.dropHoverPathNext.clear();
    ui.rowDropHovered = false;

    // A drop from another program finished last frame; the folder it hovered
    // then is the destination, or the active tab's folder when it was over
    // nothing in particular.
    if (ui.externalDrop.completed)
        {
        UiState::ExternalDrop drop = ui.externalDrop;
        ui.externalDrop = UiState::ExternalDrop{};
        if (!drop.paths.empty())
            {
            const std::string dest = drop.haveTarget ? drop.targetPath : state.active().path;
            PerformDrop(state, ui, drop.paths, dest, drop.ctrl, drop.shift);
            }
        }
    if (ui.externalDrop.active)
        {
        ui.externalDrop.haveTarget = false;
        ui.externalDrop.targetPath.clear();
        }
    if (!ImGui::IsDragDropActive() && !ui.dragPaths.empty() && !ui.externalDragRequested) ui.dragPaths.clear();
}

void PerformDrop(app::AppState& state, UiState& ui, const std::vector<std::string>& sources,
                 const std::string& dest, bool ctrl, bool shift)
{
    if (sources.empty()) return;
    std::string error;
    const app::DropAction action = app::DefaultDropAction(sources, dest, ctrl, shift);
    if (!app::DropPaths(state, sources, dest, action, error)) ShowError(ui, error);
}

void FileDragSource(app::AppState& state, UiState& ui, app::Tab& tab, int entryIndex)
{
    (void)state;
    // Default flags keep ImGui's hold-to-open: a drag parked over a tab or a
    // closed tree node opens it, the way Explorer does.
    if (!ImGui::BeginDragDropSource()) return;

    const ImGuiPayload* current = ImGui::GetDragDropPayload();
    if (current == nullptr || !current->IsDataType(kPayload))
        {
        // First frame of the drag: dragging an unselected row selects it
        // alone, dragging a selected row takes the whole selection.
        if (entryIndex >= 0 && entryIndex < (int)tab.selected.size() && !tab.selected[(size_t)entryIndex])
            app::ClickSelect(tab, app::VisiblePosOf(tab, entryIndex), false, false);
        ui.dragPaths = app::SelectedPaths(tab);
        }
    const int dummy = 0;
    ImGui::SetDragDropPayload(kPayload, &dummy, sizeof(dummy));

    // Preview: what is dragged, and what dropping here would do.
    const size_t n = ui.dragPaths.size();
    if (n == 1) ImGui::Text("%s  %s", ICON_MD_INSERT_DRIVE_FILE, app::PathName(ui.dragPaths[0]).c_str());
    else        ImGui::Text("%s  %zu items", ICON_MD_CONTENT_COPY, n);
    if (!ui.dropHoverPath.empty())
        {
        const ImGuiIO& io = ImGui::GetIO();
        const app::DropAction action = app::DefaultDropAction(ui.dragPaths, ui.dropHoverPath, io.KeyCtrl, io.KeyShift);
        ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().textMuted);
        ImGui::Text("%s to %s", action == app::DropAction::Move ? "Move" : "Copy",
                    app::LocationTitle(state, ui.dropHoverPath).c_str());
        ImGui::PopStyleColor();
        }

    // Past the window edge the drag belongs to the OS: other programs can
    // only see it as an OLE drag, which the host starts after this frame.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 m = ImGui::GetIO().MousePos;
    if (ImGui::IsMousePosValid(&m) &&
        (m.x < vp->Pos.x || m.y < vp->Pos.y || m.x >= vp->Pos.x + vp->Size.x || m.y >= vp->Pos.y + vp->Size.y))
        ui.externalDragRequested = true;

    ImGui::EndDragDropSource();
}

bool FileDropTarget(app::AppState& state, UiState& ui, const std::string& folderPath)
{
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    bool inAppHit = false;
    bool delivered = false;
    if (ImGui::BeginDragDropTarget())
        {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayload, ImGuiDragDropFlags_AcceptBeforeDelivery |
                                                                                    ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
            {
            inAppHit = true;
            delivered = payload->IsDelivery();
            }
        ImGui::EndDragDropTarget();
        }
    const bool externalHit = ui.externalDrop.active &&
                             ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                                  ImGuiHoveredFlags_AllowWhenOverlapped);
    return HandleTargetHit(state, ui, folderPath, mn, mx, inAppHit, delivered, externalHit);
}

bool FileDropTargetRect(app::AppState& state, UiState& ui, const std::string& folderPath, const ImRect& rect, ImGuiID id)
{
    bool inAppHit = false;
    bool delivered = false;
    if (ImGui::IsDragDropActive() && ImGui::BeginDragDropTargetCustom(rect, id))
        {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayload, ImGuiDragDropFlags_AcceptBeforeDelivery |
                                                                                    ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
            {
            inAppHit = true;
            delivered = payload->IsDelivery();
            }
        ImGui::EndDragDropTarget();
        }
    const bool externalHit = ui.externalDrop.active && !ui.externalDrop.haveTarget &&
                             rect.Contains(ImGui::GetIO().MousePos);
    return HandleTargetHit(state, ui, folderPath, rect.Min, rect.Max, inAppHit, delivered, externalHit);
}

} // namespace ui
