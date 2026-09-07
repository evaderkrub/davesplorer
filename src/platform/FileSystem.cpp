#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <algorithm>

namespace platform
{

namespace
{

std::wstring WithTrailingSlash(std::wstring p)
{
    if (!p.empty() && p.back() != L'\\' && p.back() != L'/') p += L'\\';
    return p;
}

int64_t FileTimeToTicks(const FILETIME& ft)
{
    ULARGE_INTEGER u;
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (int64_t)u.QuadPart;
}

// SHFileOperation wants a list of paths separated by NUL and terminated by a
// second NUL; std::wstring::c_str() supplies only the last one.
std::wstring DoubleNullList(const std::vector<std::string>& paths)
{
    std::wstring list;
    for (const std::string& p : paths)
        {
        list += Utf8ToWide(p);
        list += L'\0';
        }
    list += L'\0';
    return list;
}

bool RunShellOp(UINT func, const std::vector<std::string>& sources, const std::string& dest,
                FILEOP_FLAGS flags, std::string& error)
{
    if (sources.empty()) return true;
    std::wstring from = DoubleNullList(sources);
    std::wstring to;
    if (!dest.empty())
        {
        to = Utf8ToWide(dest);
        to += L'\0';
        to += L'\0';
        }
    SHFILEOPSTRUCTW op = {};
    op.wFunc  = func;
    op.pFrom  = from.c_str();
    op.pTo    = dest.empty() ? nullptr : to.c_str();
    op.fFlags = flags;
    const int rc = SHFileOperationW(&op);
    if (rc != 0)
        {
        // SHFileOperation returns its own code space, not GetLastError; the
        // Win32 text is right often enough to be worth showing.
        error = "The operation failed: " + WinErrorMessage((unsigned long)rc);
        return false;
        }
    if (op.fAnyOperationsAborted)
        {
        error = "The operation was cancelled.";
        return false;
        }
    return true;
}

} // namespace

std::string WinErrorMessage(unsigned long code)
{
    wchar_t* buf = nullptr;
    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, (LPWSTR)&buf, 0, nullptr);
    std::string msg = n ? WideToUtf8(std::wstring_view(buf, n)) : std::string();
    if (buf) LocalFree(buf);
    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' '))
        msg.pop_back();
    if (msg.empty()) msg = "Error " + std::to_string(code);
    return msg;
}

bool ListDirectory(const std::string& path, std::vector<FileEntry>& out, std::string& error)
{
    out.clear();
    const std::wstring dir     = WithTrailingSlash(Utf8ToWide(path));
    const std::wstring pattern = dir + L"*";
    const std::string  dirUtf8 = WideToUtf8(dir);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch,
                                nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE)
        {
        const DWORD code = GetLastError();
        // An empty directory is not an error, just an empty listing.
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_NO_MORE_FILES) return true;
        error = WinErrorMessage(code);
        return false;
        }
    do
        {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            continue;
        FileEntry e;
        e.nameWide       = fd.cFileName;
        e.name           = WideToUtf8(e.nameWide);
        e.path           = dirUtf8 + e.name;
        e.isDirectory    = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.isHidden       = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        e.isSystem       = (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
        e.isReadOnly     = (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
        e.isReparsePoint = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        e.size           = e.isDirectory ? 0 : (((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
        e.modified       = FileTimeToTicks(fd.ftLastWriteTime);
        if (!e.isDirectory)
            {
            const size_t dot = e.name.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < e.name.size())
                e.extension = ToLowerAscii(std::string_view(e.name).substr(dot + 1));
            }
        out.push_back(std::move(e));
        }
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return true;
}

std::vector<DriveInfo> GetDrives()
{
    std::vector<DriveInfo> drives;
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i)
        {
        if (!(mask & (1u << i))) continue;
        DriveInfo d;
        const wchar_t letter = (wchar_t)(L'A' + i);
        const std::wstring root = std::wstring(1, letter) + L":\\";
        d.root   = WideToUtf8(root);
        d.letter = WideToUtf8(std::wstring(1, letter) + L":");

        const UINT type = GetDriveTypeW(root.c_str());
        switch (type)
            {
            case DRIVE_REMOVABLE: d.typeName = "Removable Disk"; break;
            case DRIVE_CDROM:     d.typeName = "CD Drive"; break;
            case DRIVE_REMOTE:    d.typeName = "Network Drive"; break;
            case DRIVE_RAMDISK:   d.typeName = "RAM Disk"; break;
            default:              d.typeName = "Local Disk"; break;
            }

        // Removable drives without media would pop the "insert a disk"
        // dialog from GetVolumeInformation; SEM_FAILCRITICALERRORS keeps it
        // quiet and just fails.
        const UINT prevMode = SetErrorMode(SEM_FAILCRITICALERRORS);
        wchar_t label[MAX_PATH + 1] = {};
        d.ready = GetVolumeInformationW(root.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) != 0;
        if (d.ready)
            {
            d.label = WideToUtf8(label);
            ULARGE_INTEGER freeToCaller, total, freeTotal;
            if (GetDiskFreeSpaceExW(root.c_str(), &freeToCaller, &total, &freeTotal))
                {
                d.totalBytes = total.QuadPart;
                d.freeBytes  = freeToCaller.QuadPart;
                }
            }
        SetErrorMode(prevMode);

        d.displayName = (d.label.empty() ? d.typeName : d.label) + " (" + d.letter + ")";
        drives.push_back(std::move(d));
        }
    return drives;
}

std::vector<KnownFolder> GetKnownFolders()
{
    struct Spec
        {
        KnownFolderKind kind;
        const KNOWNFOLDERID* id;
        const char* name;
        };
    const Spec specs[] = {
        { KnownFolderKind::Desktop,   &FOLDERID_Desktop,   "Desktop"   },
        { KnownFolderKind::Downloads, &FOLDERID_Downloads, "Downloads" },
        { KnownFolderKind::Documents, &FOLDERID_Documents, "Documents" },
        { KnownFolderKind::Pictures,  &FOLDERID_Pictures,  "Pictures"  },
        { KnownFolderKind::Music,     &FOLDERID_Music,     "Music"     },
        { KnownFolderKind::Videos,    &FOLDERID_Videos,    "Videos"    },
        { KnownFolderKind::Home,      &FOLDERID_Profile,   "Home"      },
    };
    std::vector<KnownFolder> out;
    for (const Spec& s : specs)
        {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*s.id, KF_FLAG_DEFAULT, nullptr, &p)) && p)
            {
            KnownFolder kf;
            kf.kind = s.kind;
            kf.name = s.name;
            kf.path = WideToUtf8(p);
            out.push_back(std::move(kf));
            }
        if (p) CoTaskMemFree(p);
        }
    return out;
}

bool PathExists(const std::string& path)
{
    return GetFileAttributesW(Utf8ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectory(const std::string& path)
{
    const DWORD a = GetFileAttributesW(Utf8ToWide(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

std::string FileTypeName(const std::string& extension, bool isDirectory)
{
    // USEFILEATTRIBUTES makes the shell answer from the registry alone, so a
    // fake file name is enough and nothing touches the disk.
    const std::wstring fake = isDirectory ? L"folder" : Utf8ToWide("file." + extension);
    SHFILEINFOW info = {};
    const DWORD attrs = isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (SHGetFileInfoW(fake.c_str(), attrs, &info, sizeof(info), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
        return WideToUtf8(info.szTypeName);
    if (isDirectory) return "File folder";
    if (extension.empty()) return "File";
    std::string upper = extension;
    for (char& c : upper) c = (char)toupper((unsigned char)c);
    return upper + " File";
}

std::string FormatDateTime(int64_t ticks)
{
    FILETIME ft;
    ft.dwLowDateTime  = (DWORD)(ticks & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)((uint64_t)ticks >> 32);
    SYSTEMTIME utc, local;
    if (!FileTimeToSystemTime(&ft, &utc)) return {};
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return {};
    wchar_t date[64] = {}, time[64] = {};
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, 64, nullptr);
    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, 64);
    return WideToUtf8(date) + " " + WideToUtf8(time);
}

bool CreateFolder(const std::string& path, std::string& error)
{
    if (CreateDirectoryW(Utf8ToWide(path).c_str(), nullptr)) return true;
    error = WinErrorMessage(GetLastError());
    return false;
}

bool CreateEmptyFile(const std::string& path, std::string& error)
{
    HANDLE h = CreateFileW(Utf8ToWide(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        {
        error = WinErrorMessage(GetLastError());
        return false;
        }
    CloseHandle(h);
    return true;
}

bool RenamePath(const std::string& from, const std::string& to, std::string& error)
{
    if (MoveFileExW(Utf8ToWide(from).c_str(), Utf8ToWide(to).c_str(), 0)) return true;
    error = WinErrorMessage(GetLastError());
    return false;
}

bool DeletePermanently(const std::vector<std::string>& paths, std::string& error)
{
    return RunShellOp(FO_DELETE, paths, "", FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, error);
}

bool MoveToRecycleBin(const std::vector<std::string>& paths, std::string& error)
{
    return RunShellOp(FO_DELETE, paths, "", FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI, error);
}

bool CopyPaths(const std::vector<std::string>& sources, const std::string& destDir,
               bool renameOnCollision, std::string& error)
{
    FILEOP_FLAGS flags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;
    if (renameOnCollision) flags |= FOF_RENAMEONCOLLISION;
    return RunShellOp(FO_COPY, sources, destDir, flags, error);
}

bool MovePaths(const std::vector<std::string>& sources, const std::string& destDir, std::string& error)
{
    return RunShellOp(FO_MOVE, sources, destDir, FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR, error);
}

} // namespace platform
