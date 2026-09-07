// Font loading: Open Sans for the interface, Fira Code for anything
// column-aligned, Material Icons merged into the text faces.
//
// With ImGui 1.92 a font is a face, not a face-at-a-size: glyphs rasterise on
// demand, so there is one face per style and the size is a PushFont argument.
#pragma once

#include "imgui.h"

#include <string>

namespace ui::fonts
{

// Loads from <assetsDir>/fonts. Falls back to ImGui's built-in font when a
// file is missing, so the app still runs; the error says which file.
bool Load(const std::string& assetsDir, float sizePx, std::string& error);

ImFont* Ui();
ImFont* Bold();
ImFont* Italic();
ImFont* Mono();
float   Size();

} // namespace ui::fonts
