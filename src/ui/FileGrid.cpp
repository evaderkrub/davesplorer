// The thumbnail grid: a folder's contents as pictures (image files) and
// big icons (everything else) with the name underneath, Explorer's icon
// views. Same items, selection, lasso, drag and menus as the details
// table; only the layout differs.
#include "ui/FileList.h"
#include "ui/DragDrop.h"
#include "ui/Fonts.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Thumbnails.h"
#include "ui/Widgets.h"

#include "app/Image.h"
#include "app/Selection.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

namespace ui
{

namespace
{

// The name under a cell: up to two lines, centred, the second cut with
// an ellipsis. The full name is in the tooltip.
void DrawCellName(ImDrawList* dl, ImVec2 topLeft, float width, const std::string& text, ImU32 color)
{
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const float lineH = ImGui::GetTextLineHeight();
    const char* s = text.c_str();
    const char* end = s + text.size();

    const char* split = font->CalcWordWrapPosition(size, s, end, width);
    if (split <= s)
        {
        // Not even one character fits by the wrap rules: take one anyway
        // (a whole UTF-8 sequence) rather than draw nothing.
        split = s + 1;
        while (split < end && ((unsigned char)*split & 0xC0) == 0x80) ++split;
        }
    const char* line1End = split;
    while (line1End > s && line1End[-1] == ' ') --line1End;
    const char* line2 = split;
    while (line2 < end && *line2 == ' ') ++line2;

    const float w1 = ImGui::CalcTextSize(s, line1End).x;
    dl->AddText(ImVec2(topLeft.x + std::max(0.0f, (width - w1) * 0.5f), topLeft.y), color, s, line1End);
    if (line2 >= end) return;

    const ImVec2 pos2(topLeft.x, topLeft.y + lineH);
    const float w2 = ImGui::CalcTextSize(line2, end).x;
    if (w2 <= width)
        {
        dl->AddText(ImVec2(pos2.x + (width - w2) * 0.5f, pos2.y), color, line2, end);
        return;
        }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::RenderTextEllipsis(dl, pos2, ImVec2(pos2.x + width, pos2.y + lineH), pos2.x + width, line2, end, nullptr);
    ImGui::PopStyleColor();
}

} // namespace

void DrawThumbnailGrid(app::AppState& state, UiState& ui, int tabIndex)
{
    app::Tab& tab = state.tabs[(size_t)tabIndex];
    ViewUi& v = ui.view(tab.id);
    const app::Settings& settings = state.settings;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float scale = style.FontScaleMain * style.FontScaleDpi;

    // A cell is the picture box with padding, and two lines of name.
    const float thumb = (float)settings.thumbnailSize * scale;
    const float pad = 6.0f * scale;
    const float lineH = ImGui::GetTextLineHeight();
    const ImVec2 cell(thumb + pad * 2.0f, thumb + pad * 2.0f + lineH * 2.0f + pad);

    // The same empty strip on the right as the table keeps, so a
    // rubber-band always has somewhere to start.
    const float lassoMargin = 24.0f * scale;
    const float width = std::max(ImGui::GetContentRegionAvail().x - lassoMargin, cell.x + style.ItemSpacing.x);
    ImGui::BeginChild("FileGrid", ImVec2(width, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);

    const float inner = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, (int)((inner + style.ItemSpacing.x) / (cell.x + style.ItemSpacing.x)));
    const int count = (int)tab.visible.size();
    const int rows = (count + cols - 1) / cols;
    v.gridColumns = cols;
    v.gridRowsVisible = std::max(1, (int)(ImGui::GetContentRegionAvail().y / (cell.y + style.ItemSpacing.y)));

    NoticeNewListing(tab, v);
    const ImRect lassoRect = UpdateLasso(tab, v);

    const Palette& palette = CurrentPalette();
    const ImVec4 selectedBg = ImGui::ColorConvertU32ToFloat4(WithAlpha(palette.accent, 70));
    const ImVec4 hoveredBg  = ImGui::ColorConvertU32ToFloat4(WithAlpha(palette.accent, 40));
    ImGui::PushStyleColor(ImGuiCol_Header, selectedBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoveredBg);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, selectedBg);

    ListClicks clicks;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* iconFont = fonts::Ui();   // the Material icons are merged into it

    ImGuiListClipper clipper;
    clipper.Begin(rows, cell.y + style.ItemSpacing.y);
    if (v.scrollToEntry >= 0)
        {
        const int pos = app::VisiblePosOf(tab, v.scrollToEntry);
        if (pos >= 0) clipper.IncludeItemByIndex(pos / cols);
        }
    while (clipper.Step())
        {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
            for (int col = 0; col < cols; ++col)
                {
                const int pos = row * cols + col;
                if (pos >= count) break;
                const int entryIndex = tab.visible[(size_t)pos];
                const platform::FileEntry& e = tab.entries[(size_t)entryIndex];
                const bool isSelected = entryIndex < (int)tab.selected.size() && tab.selected[(size_t)entryIndex];

                if (col > 0) ImGui::SameLine();
                if (v.scrollToEntry == entryIndex)
                    {
                    ImGui::SetScrollHereY(0.5f);
                    v.scrollToEntry = -1;
                    }

                // As in the table, the selectable carries only the ID (the
                // file name, which is what the tests refer to); the
                // picture and name are drawn over it.
                const ImVec2 cellMin = ImGui::GetCursorScreenPos();
                const std::string label = "###" + e.name;
                const bool clicked = ImGui::Selectable(label.c_str(), isSelected,
                    ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, cell);
                const bool hoveredNow = ImGui::IsItemHovered();
                ApplyLassoToItem(tab, v, entryIndex, lassoRect, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), false);
                if (!tab.path.empty()) FileDragSource(state, ui, tab, entryIndex);
                if (e.isDirectory && FileDropTarget(state, ui, e.path)) ui.rowDropHovered = true;

                const bool isCut = state.clipboard.cut &&
                    std::find(state.clipboard.paths.begin(), state.clipboard.paths.end(), e.path) != state.clipboard.paths.end();
                const bool ghost = e.isHidden || isCut;
                const int alpha = ghost ? 110 : 255;

                // The picture box: a thumbnail for an image, otherwise the
                // entry's icon drawn large.
                const ImVec2 boxMin(cellMin.x + pad, cellMin.y + pad);
                const ImVec2 boxCenter(boxMin.x + thumb * 0.5f, boxMin.y + thumb * 0.5f);
                const bool isImage = !e.isDirectory && app::IsViewableImage(e.extension);
                const thumbnails::Thumb* t = isImage ? thumbnails::Get(e.path, e.modified) : nullptr;
                if (t && t->texture)
                    {
                    // Fit the file's real size into the box, shrinking but
                    // never enlarging beyond the interface scale, so a
                    // 16-pixel icon stays a small thing rather than a blur.
                    const float sw = (float)std::max(t->sourceWidth, 1), sh = (float)std::max(t->sourceHeight, 1);
                    const float fit = std::min(std::min(thumb / sw, thumb / sh), scale);
                    const ImVec2 half(sw * fit * 0.5f, sh * fit * 0.5f);
                    const ImVec2 p0(std::floor(boxCenter.x - half.x), std::floor(boxCenter.y - half.y));
                    const ImVec2 p1(p0.x + std::floor(half.x * 2.0f), p0.y + std::floor(half.y * 2.0f));
                    dl->AddImage(t->texture->GetTexRef(), p0, p1, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
                    }
                else
                    {
                    const char* icon = (t && t->failed) ? ICON_MD_BROKEN_IMAGE : IconForEntry(e);
                    ImU32 tint = IconTintForEntry(e);
                    if (isImage && !t) tint = WithAlpha(tint, 90);   // still decoding
                    const float glyph = thumb * 0.7f;
                    const ImVec2 gs = iconFont->CalcTextSizeA(glyph, FLT_MAX, 0.0f, icon);
                    dl->AddText(iconFont, glyph, ImVec2(boxCenter.x - gs.x * 0.5f, boxCenter.y - gs.y * 0.5f),
                                WithAlpha(tint, alpha), icon);
                    }

                DrawCellName(dl, ImVec2(cellMin.x + pad, boxMin.y + thumb + pad * 0.5f), cell.x - pad * 2.0f,
                             ShownName(tab, e, settings), ghost ? palette.textFaint : palette.text);

                if (hoveredNow) clicks.itemHovered = true;
                if (clicked)
                    {
                    clicks.pendingClick = pos;
                    clicks.pendingDouble = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) clicks.pendingContext = pos;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNormal))
                    {
                    // Names are cut short in a cell, so the tooltip has the
                    // whole one.
                    if (e.isReparsePoint) ImGui::SetTooltip("%s\nLink: %s", e.name.c_str(), e.path.c_str());
                    else ImGui::SetTooltip("%s", e.name.c_str());
                    }
                }
            }
        }
    ImGui::PopStyleColor(3);

    DrawLassoBand(v, lassoRect);
    EndLassoOnRelease(tab, v);
    ResolveListClicks(state, ui, tabIndex, clicks);
    DrawListPopups(state, ui, tabIndex);
    ImGui::EndChild();
}

} // namespace ui
