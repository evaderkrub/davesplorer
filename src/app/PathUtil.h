// Native path manipulation for Windows and Linux. Nothing here touches the disk,
// so it is testable without one.
//
// The empty string is a real location: "This PC", the virtual root listing
// the drives. Every function treats it that way.
#pragma once

#include <string>
#include <vector>

namespace app
{

// Backslashes, no trailing separator except on a drive root ("C:\").
// "C:" becomes "C:\". Forward slashes are accepted on input.
std::string NormalizePath(const std::string& path);

// Parent of "C:\Users\dave" is "C:\Users"; of "C:\Users" is "C:\"; of
// "C:\" is "" (This PC); of "" is "".
std::string ParentPath(const std::string& path);

// Last component; "C:\" for a drive root, "This PC" for the virtual root.
std::string PathName(const std::string& path);

std::string JoinPath(const std::string& dir, const std::string& name);

bool IsDriveRoot(const std::string& path);

struct Crumb
{
    std::string label;   // "C:" for drive roots; the interface swaps in the drive's display name
    std::string path;    // what to navigate to when clicked
};

// "" -> [This PC]; "C:\Users\dave" -> [This PC, C:, Users, dave].
std::vector<Crumb> Breadcrumbs(const std::string& path);

// A name a user typed for a new file or folder: rejects separators, the
// reserved characters and the reserved device names.
bool IsValidFileName(const std::string& name, std::string& reason);

} // namespace app
