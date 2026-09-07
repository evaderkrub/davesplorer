// The shortcut bar under the menu: ten slots, empty until clicked. A click
// on an empty slot pins the selected folder; from then on the slot jumps
// the active view there. Right-click for Open / Assign / Clear.
#include "ui/MainWindow.h"
#include "ui/DragDrop.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Shortcuts.h"

#include "imgui.h"

#include <algorithm>
#include <string>

namespace ui
{

namespace
{

constexpr ImU32 kFolderRed = IM_COL32(214, 52, 52, 255);   // the app icon's red

void JumpTo(app::AppState& state, UiState& ui, const std::string& path, bool newView)
{
    if (newView)
        {
        OpenView(state, ui, path);
        return;
        }
    std::string error;
    if (!app::NavigateTo(state.active(), path, error)) ShowError(ui, error);
}

} // namespace

void DrawShortcutBar(app::AppState& state, UiState& ui)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const Palette& p = CurrentPalette();
    const float pad = style.FramePadding.y;
    const float barHeight = ImGui::GetFrameHeight() + pad * 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.surface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.ItemSpacing.x, pad));
    ImGui::BeginChild("##shortcutbar", ImVec2(0, barHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Equal-width slots filling the bar.
    const int n = app::kShortcutSlots;
    const float spacing = style.ItemSpacing.x;
    const float slotW = std::max((ImGui::GetContentRegionAvail().x - spacing * (float)(n - 1)) / (float)n, 40.0f);
    const std::string candidate = app::ShortcutCandidate(state.active());

    for (int slot = 0; slot < n; ++slot)
        {
        if (slot > 0) ImGui::SameLine();
        const bool assigned = app::ShortcutAssigned(state, slot);
        const std::string path = app::ShortcutPath(state, slot);
        const std::string id = "###shortcut" + std::to_string(slot);

        if (assigned)
            {
            // Icon in the app's red, name in text color, both clipped to the slot.
            const std::string label = std::string(ICON_MD_FOLDER) + " " + app::LocationTitle(state, path) + id;
            ImGui::PushStyleColor(ImGuiCol_Text, kFolderRed);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Button(label.c_str(), ImVec2(slotW, 0));
            ImGui::PopStyleColor();
            // Repaint the name in text color over the red label.
            const float iconW = ImGui::CalcTextSize(ICON_MD_FOLDER " ").x;
            const ImVec2 textPos(start.x + style.FramePadding.x + iconW, start.y + style.FramePadding.y);
            const ImVec4 clip(start.x, start.y, start.x + slotW - style.FramePadding.x, start.y + ImGui::GetFrameHeight());
            ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, textPos, p.text, app::LocationTitle(state, path).c_str(),
                                                nullptr, 0.0f, &clip);
            if (clicked) JumpTo(state, ui, path, ImGui::GetIO().KeyCtrl);
            TipOnHover(path.c_str());
            FileDropTarget(state, ui, path);
            }
        else
            {
            // An empty slot: a faint outline, a "+" only while hovered.
            ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(p.surfaceHovered, 40));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(p.surfaceHovered, 120));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(p.surfaceActive, 160));
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Button(id.c_str(), ImVec2(slotW, 0));
            ImGui::PopStyleColor(3);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRect(start, ImVec2(start.x + slotW, start.y + ImGui::GetFrameHeight()), WithAlpha(p.border, 120),
                        style.FrameRounding);
            if (ImGui::IsItemHovered())
                {
                const ImVec2 plusSize = ImGui::CalcTextSize(ICON_MD_ADD);
                dl->AddText(ImVec2(start.x + (slotW - plusSize.x) * 0.5f, start.y + (ImGui::GetFrameHeight() - plusSize.y) * 0.5f),
                            p.textFaint, ICON_MD_ADD);
                if (!candidate.empty()) TipOnHover(("Pin " + app::LocationTitle(state, candidate) + " here").c_str());
                }
            if (clicked && !candidate.empty()) app::AssignShortcut(state, slot, candidate);
            }

        if (ImGui::BeginPopupContextItem(("shortcutmenu" + std::to_string(slot)).c_str()))
            {
            if (assigned)
                {
                MutedText(path.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Open")) JumpTo(state, ui, path, false);
                if (ImGui::MenuItem("Open in new view")) JumpTo(state, ui, path, true);
                ImGui::Separator();
                }
            if (!candidate.empty() && candidate != path &&
                ImGui::MenuItem(("Assign " + app::LocationTitle(state, candidate)).c_str()))
                app::AssignShortcut(state, slot, candidate);
            if (ImGui::MenuItem("Clear", nullptr, false, assigned)) app::ClearShortcut(state, slot);
            ImGui::EndPopup();
            }
        }
    ImGui::EndChild();
}

} // namespace ui
