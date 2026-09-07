// Small drawing helpers shared by the panes.
#pragma once

#include "imgui.h"
#include "platform/FileSystem.h"

namespace ui
{

// A flat toolbar button showing one Material icon. The id is what tests and
// ImGui see; the icon is only what the user sees.
bool IconButton(const char* id, const char* icon, const char* tooltip, bool enabled = true);

// Tooltip on the last item, after the usual hover delay.
void TipOnHover(const char* text);

void MutedText(const char* text);
void FaintText(const char* text);

// Icon and tint for a file-list entry, chosen by extension.
const char* IconForEntry(const platform::FileEntry& entry);
ImU32       IconTintForEntry(const platform::FileEntry& entry);

const char* IconForKnownFolder(platform::KnownFolderKind kind);

} // namespace ui
