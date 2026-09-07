#include "ui/Fonts.h"
#include "ui/IconsMaterialDesign.h"
#include "platform/FileSystem.h"

namespace ui::fonts
{

namespace
{

ImFont* g_ui = nullptr;
ImFont* g_bold = nullptr;
ImFont* g_italic = nullptr;
ImFont* g_mono = nullptr;
float   g_size = 18.0f;

// ImGui keeps a pointer to these arrays, so they must outlive the atlas.
const ImWchar kTextRanges[] = {
    0x0020, 0x00FF,   // Basic Latin + Latin Supplement
    0x0100, 0x017F,   // Latin Extended-A, for European file names
    0x2000, 0x206F,   // General Punctuation (bullet, dashes, ellipsis)
    0x2190, 0x21FF,   // Arrows
    0x2200, 0x22FF,   // Mathematical Operators
    0,
};
const ImWchar kIconRanges[] = { ICON_MIN_MD, ICON_MAX_16_MD, 0 };

ImFont* AddText(ImGuiIO& io, const std::string& file, float size)
{
    if (!platform::PathExists(file)) return nullptr;
    ImFontConfig cfg;
    return io.Fonts->AddFontFromFileTTF(file.c_str(), size, &cfg, kTextRanges);
}

void MergeIcons(ImGuiIO& io, const std::string& file, float size)
{
    if (!platform::PathExists(file)) return;
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.PixelSnapH = true;
    // Give every icon a full em advance so icon columns line up, and nudge
    // them down: Material glyphs sit high on the text line and look
    // top-heavy inside framed buttons otherwise.
    cfg.GlyphMinAdvanceX = size;
    cfg.GlyphOffset.y = size * 0.18f;
    io.Fonts->AddFontFromFileTTF(file.c_str(), size, &cfg, kIconRanges);
}

} // namespace

bool Load(const std::string& assetsDir, float sizePx, std::string& error)
{
    ImGuiIO& io = ImGui::GetIO();
    g_size = sizePx;
    const std::string dir = assetsDir + "\\fonts\\";
    const std::string icons = dir + "MaterialIcons-Regular.ttf";

    g_ui = AddText(io, dir + "OpenSans-Regular.ttf", sizePx);
    if (!g_ui)
        {
        error = "Missing font: " + dir + "OpenSans-Regular.ttf (using the built-in font)";
        g_ui = io.Fonts->AddFontDefault();
        g_bold = g_italic = g_mono = g_ui;
        return false;
        }
    MergeIcons(io, icons, sizePx);

    g_bold = AddText(io, dir + "OpenSans-SemiBold.ttf", sizePx);
    if (g_bold) MergeIcons(io, icons, sizePx); else g_bold = g_ui;

    g_italic = AddText(io, dir + "OpenSans-Italic.ttf", sizePx);
    if (g_italic) MergeIcons(io, icons, sizePx); else g_italic = g_ui;

    g_mono = AddText(io, dir + "FiraCode-Regular.ttf", sizePx);
    if (!g_mono) g_mono = g_ui;

    io.FontDefault = g_ui;
    return true;
}

ImFont* Ui()     { return g_ui; }
ImFont* Bold()   { return g_bold; }
ImFont* Italic() { return g_italic; }
ImFont* Mono()   { return g_mono; }
float   Size()   { return g_size; }

} // namespace ui::fonts
