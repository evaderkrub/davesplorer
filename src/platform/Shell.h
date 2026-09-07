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

// A command prompt in the given folder.
bool OpenTerminalAt(const std::string& dir, std::string& error);

} // namespace platform
