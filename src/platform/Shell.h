// Handing things to the Windows shell: launching files, revealing them in
// Explorer, opening a terminal.
#pragma once

#include <string>

namespace platform
{

// Opens a file with its associated program, or a folder in Explorer.
bool OpenWithShell(const std::string& path, std::string& error);

// Windows Explorer with the item selected.
bool ShowInExplorer(const std::string& path, std::string& error);

// Windows Explorer showing a folder ("" for This PC). Always explorer.exe
// itself: once Davesplorer is registered as the folder handler, a plain
// "open the folder" request would come straight back here.
bool OpenFolderInExplorer(const std::string& folder, std::string& error);

// A command prompt in the given folder.
bool OpenTerminalAt(const std::string& dir, std::string& error);

} // namespace platform
