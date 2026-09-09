#include "platform/Shell.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

namespace platform
{

namespace
{

bool Execute(const wchar_t* verb, const std::wstring& file, const std::wstring& args,
             const std::wstring& dir, std::string& error)
{
    // ShellExecute returns an HINSTANCE-shaped integer; anything above 32 is
    // success and the small values are error codes.
    const INT_PTR rc = (INT_PTR)ShellExecuteW(nullptr, verb, file.c_str(),
                                              args.empty() ? nullptr : args.c_str(),
                                              dir.empty() ? nullptr : dir.c_str(), SW_SHOWNORMAL);
    if (rc > 32) return true;
    switch (rc)
        {
        case SE_ERR_NOASSOC:   error = "No program is associated with this file type."; break;
        case SE_ERR_ACCESSDENIED: error = "Access denied."; break;
        case ERROR_FILE_NOT_FOUND: error = "The file could not be found."; break;
        case ERROR_PATH_NOT_FOUND: error = "The path could not be found."; break;
        default:               error = WinErrorMessage((unsigned long)rc); break;
        }
    return false;
}

// The Explorer to launch for "show me the real thing". Normally just
// explorer.exe; but once the interceptor shim is installed as the IFEO
// Debugger for explorer.exe, launching it by name lands right back in
// Davesplorer. The shim keeps a private copy of the real Explorer under a
// different name beside itself (winexplorer_real.exe) for its own
// pass-through; we use the same copy.
std::wstring RealExplorer()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\explorer.exe",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return L"explorer.exe";
    wchar_t buf[2048] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, L"Debugger", nullptr, &type, reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || !buf[0]) return L"explorer.exe";

    // The value is `"C:\...\explorer_shim.exe"`, possibly unquoted. Only the
    // directory matters; the copy lives beside the shim.
    std::wstring shim(buf);
    if (shim.front() == L'"')
        {
        const size_t close = shim.find(L'"', 1);
        shim = close == std::wstring::npos ? shim.substr(1) : shim.substr(1, close - 1);
        }
    const size_t slash = shim.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"explorer.exe";

    const std::wstring copy = shim.substr(0, slash) + L"\\winexplorer_real.exe";
    if (!PathFileExistsW(copy.c_str()))
        {
        // The shim re-creates a missing copy the same way; do it here too so
        // the button works even before the shim has run once.
        wchar_t win[MAX_PATH] = {};
        GetWindowsDirectoryW(win, MAX_PATH);
        CopyFileW((std::wstring(win) + L"\\explorer.exe").c_str(), copy.c_str(), FALSE);
        if (!PathFileExistsW(copy.c_str())) return L"explorer.exe";
        }
    return copy;
}

} // namespace

bool OpenWithShell(const std::string& path, std::string& error)
{
    return Execute(L"open", Utf8ToWide(path), L"", L"", error);
}

bool ShowInExplorer(const std::string& path, std::string& error)
{
    return Execute(nullptr, RealExplorer(), L"/select,\"" + Utf8ToWide(path) + L"\"", L"", error);
}

bool OpenFolderInExplorer(const std::string& folder, std::string& error)
{
    const std::wstring arg = folder.empty() ? L"shell:MyComputerFolder" : L"\"" + Utf8ToWide(folder) + L"\"";
    return Execute(nullptr, RealExplorer(), arg, L"", error);
}

bool OpenTerminalAt(const std::string& dir, std::string& error)
{
    // Windows Terminal when installed, otherwise the classic prompt.
    std::string ignored;
    if (Execute(nullptr, L"wt.exe", L"-d \"" + Utf8ToWide(dir) + L"\"", L"", ignored)) return true;
    return Execute(nullptr, L"cmd.exe", L"", Utf8ToWide(dir), error);
}

} // namespace platform
