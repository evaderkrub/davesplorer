#include "platform/Shell.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>
#include <shellapi.h>

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

} // namespace

bool OpenWithShell(const std::string& path, std::string& error)
{
    return Execute(L"open", Utf8ToWide(path), L"", L"", error);
}

bool ShowInExplorer(const std::string& path, std::string& error)
{
    return Execute(nullptr, L"explorer.exe", L"/select,\"" + Utf8ToWide(path) + L"\"", L"", error);
}

bool OpenFolderInExplorer(const std::string& folder, std::string& error)
{
    const std::wstring arg = folder.empty() ? L"shell:MyComputerFolder" : L"\"" + Utf8ToWide(folder) + L"\"";
    return Execute(nullptr, L"explorer.exe", arg, L"", error);
}

bool OpenTerminalAt(const std::string& dir, std::string& error)
{
    // Windows Terminal when installed, otherwise the classic prompt.
    std::string ignored;
    if (Execute(nullptr, L"wt.exe", L"-d \"" + Utf8ToWide(dir) + L"\"", L"", ignored)) return true;
    return Execute(nullptr, L"cmd.exe", L"", Utf8ToWide(dir), error);
}

} // namespace platform
