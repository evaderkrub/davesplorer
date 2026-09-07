// The Windows clipboard as Explorer uses it: a CF_HDROP file list plus the
// "Preferred DropEffect" that says whether the files were cut or copied.
// Files copied in Explorer paste here and files copied here paste in
// Explorer.
#pragma once

#include <string>
#include <vector>

namespace platform
{

bool SetClipboardFiles(const std::vector<std::string>& paths, bool cut, std::string& error);

// False when the clipboard holds no file list.
bool GetClipboardFiles(std::vector<std::string>& paths, bool& cut);

// Cheap enough to ask every frame.
bool ClipboardHasFiles();

// Changes whenever anyone writes the clipboard; lets the app notice that
// its own cut list has been superseded.
unsigned long ClipboardSequence();

bool ClearClipboard();

} // namespace platform
