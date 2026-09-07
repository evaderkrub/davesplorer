// Token-based theming, carried over from fwcom.
//
// A theme is 17 semantic colors. Every ImGui style color, the style metrics
// and the widget palette derive from them, so adding a theme means picking
// 17 colors rather than maintaining a 50-entry list and hoping nothing was
// missed.
#pragma once

#include "imgui.h"

namespace ui
{

struct ThemeTokens
{
    const char* name;
    bool        isDark;           // chooses the ImGui base style to layer over

    ImU32 background;             // window bg
    ImU32 surface;                // panels, child regions
    ImU32 surfaceInput;           // text fields, buttons
    ImU32 surfacePopup;           // menus, combos, tooltips
    ImU32 border;

    ImU32 text;
    ImU32 textMuted;
    ImU32 textFaint;

    ImU32 accent;
    ImU32 link;

    ImU32 codeBackground;
    ImU32 inlineCodeBackground;
    ImU32 inlineCodeText;

    ImU32 selection;              // carries its own alpha; drawn over content

    ImU32 success;
    ImU32 warning;
    ImU32 danger;
};

// Colors the widget helpers draw with. Rebuilt whenever a theme is applied.
struct Palette
{
    ImU32 text, textMuted, textFaint;
    ImU32 heading;
    ImU32 rule;
    ImU32 link;

    ImU32 surface, surfaceHovered, surfaceActive;
    ImU32 border, borderHovered;

    ImU32 accent, accentHovered, accentActive, accentText;

    ImU32 success, warning, danger, neutral;
    ImU32 successFill, warningFill, dangerFill, neutralFill, accentFill;

    // File-list icon tints, chosen per theme so a yellow folder still reads
    // on a light background.
    ImU32 iconFolder, iconDrive, iconFile, iconImage, iconMedia, iconCode, iconArchive, iconExecutable;
};

int                 ThemeCount();
const ThemeTokens&  ThemeAt(int index);      // clamps to range
int                 ThemeIndexByName(const char* name);
int                 CurrentThemeIndex();
const Palette&      CurrentPalette();

// Blend toward white on dark themes, toward black on light ones, so one
// "slightly brighter on hover" rule reads correctly in both directions.
ImU32 Lift(ImU32 color, float amount, bool isDark);
ImU32 WithAlpha(ImU32 color, int alpha);

float ClampUiScale(float scale);

// Rebuilds the whole ImGui style from the theme's tokens, then scales metrics
// by uiScale * dpiScale and fonts through the style's FontScale fields.
// Sizes are reset from constants on every call, so this is idempotent
// (ScaleAllSizes on its own is not). Call between frames only.
void ApplyAppearanceNow(int theme, float uiScale, float dpiScale);

// Re-applies only when theme, scale or DPI changed since the last apply.
// Cheap to call every frame, immediately before ImGui::NewFrame().
void PumpAppearance(int theme, float uiScale, float dpiScale);

} // namespace ui
