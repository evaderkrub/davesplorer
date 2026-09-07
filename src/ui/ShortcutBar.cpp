// The shortcut bar under the menu: ten slots, each showing a red "+" until
// clicked. A click on an empty slot pins the selected folder; from then on
// the slot jumps the active view there. Right-click for Open / Assign / Clear.
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
            // The button carries only the ID; icon (in the app's red) and
            // name (text color) are drawn by hand so they can differ in
            // color, centered when they fit and clipped from the right
            // when they do not.
            const std::string name = app::LocationTitle(state, path);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Button(id.c_str(), ImVec2(slotW, 0));
            const float gap = style.ItemInnerSpacing.x;
            const float iconW = ImGui::CalcTextSize(ICON_MD_FOLDER).x;
            const float nameW = ImGui::CalcTextSize(name.c_str()).x;
            const float inner = slotW - style.FramePadding.x * 2.0f;
            const float contentW = iconW + gap + nameW;
            const float x0 = start.x + style.FramePadding.x + (contentW < inner ? (inner - contentW) * 0.5f : 0.0f);
            const float y0 = start.y + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f;
            const ImVec4 clip(start.x + style.FramePadding.x, start.y, start.x + slotW - style.FramePadding.x,
                              start.y + ImGui::GetFrameHeight());
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(nullptr, 0.0f, ImVec2(x0, y0), kFolderRed, ICON_MD_FOLDER, nullptr, 0.0f, &clip);
            dl->AddText(nullptr, 0.0f, ImVec2(x0 + iconW + gap, y0), p.text, name.c_str(), nullptr, 0.0f, &clip);
            if (clicked) JumpTo(state, ui, path, ImGui::GetIO().KeyCtrl);
            TipOnHover(path.c_str());
            FileDropTarget(state, ui, path);
            }
        else
            {
            // An empty slot: a faint outline, a "+" only while hovered.
            ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(p.surfaceHovered, 40));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(p.surfaceHovered, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(p.surfaceActive, 255));
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::Button(id.c_str(), ImVec2(slotW, 0));
            ImGui::PopStyleColor(3);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRect(start, ImVec2(start.x + slotW, start.y + ImGui::GetFrameHeight()), WithAlpha(p.border, 120),
                        style.FrameRounding);
            // The "+" is the pin-here invitation, always visible so an empty
            // slot is discoverable, in the same red as the pinned folder icon.
            const ImVec2 plusSize = ImGui::CalcTextSize(ICON_MD_ADD);
            dl->AddText(ImVec2(start.x + (slotW - plusSize.x) * 0.5f, start.y + (ImGui::GetFrameHeight() - plusSize.y) * 0.5f),
                        kFolderRed, ICON_MD_ADD);
            if (ImGui::IsItemHovered() && !candidate.empty())
                TipOnHover(("Pin " + app::LocationTitle(state, candidate) + " here").c_str());
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
