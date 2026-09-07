// Directory listing, drives, known folders and file operations on Win32.
//
// Failures come back as false plus a UTF-8 message the interface can show.
// No exceptions cross this boundary.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform
{

struct FileEntry
{
    std::string  name;          // UTF-8 display name
    std::wstring nameWide;      // kept for natural-order sorting without re-conversion
    std::string  path;          // full UTF-8 path
    std::string  extension;     // lower-case, no dot, empty for folders
    uint64_t     size = 0;
    int64_t      modified = 0;  // FILETIME ticks (100ns since 1601), UTC
    bool         isDirectory = false;
    bool         isHidden = false;
    bool         isSystem = false;
    bool         isReadOnly = false;
    bool         isReparsePoint = false;
};

struct DriveInfo
{
    std::string root;           // "C:\"
    std::string letter;         // "C:"
    std::string label;          // volume label, may be empty
    std::string displayName;    // "Local Disk (C:)" the way Explorer shows it
    std::string typeName;       // "Local Disk", "Removable Disk", "CD Drive", "Network Drive"
    uint64_t    totalBytes = 0;
    uint64_t    freeBytes = 0;
    bool        ready = false;  // media present and readable
};

enum class KnownFolderKind
{
    Home,
    Desktop,
    Documents,
    Downloads,
    Pictures,
    Music,
    Videos,
};

struct KnownFolder
{
    KnownFolderKind kind;
    std::string     name;
    std::string     path;
};

bool ListDirectory(const std::string& path, std::vector<FileEntry>& out, std::string& error);

std::vector<DriveInfo>   GetDrives();
std::vector<KnownFolder> GetKnownFolders();

bool PathExists(const std::string& path);
bool IsDirectory(const std::string& path);

// "Text Document", "File folder", "PNG File"... as the shell names them.
std::string FileTypeName(const std::string& extension, bool isDirectory);

// Local time in the user's short date and time format, like Explorer's
// "Date modified" column.
std::string FormatDateTime(int64_t fileTimeTicks);

// Human-readable message for a Win32 error code.
std::string WinErrorMessage(unsigned long code);

bool CreateFolder(const std::string& path, std::string& error);
bool CreateEmptyFile(const std::string& path, std::string& error);
bool RenamePath(const std::string& from, const std::string& to, std::string& error);

bool DeletePermanently(const std::vector<std::string>& paths, std::string& error);
bool MoveToRecycleBin(const std::vector<std::string>& paths, std::string& error);

// Shell copy/move: the OS shows its own progress and conflict dialogs, which
// is exactly the behaviour Explorer users expect. renameOnCollision makes a
// copy into the same folder produce "name - Copy".
bool CopyPaths(const std::vector<std::string>& sources, const std::string& destDir,
               bool renameOnCollision, std::string& error);
bool MovePaths(const std::vector<std::string>& sources, const std::string& destDir, std::string& error);

} // namespace platform
