// The Explorer interceptor.
//
// Installed as the Image File Execution Options "Debugger" for explorer.exe,
// so Windows runs THIS in place of every explorer.exe launch, from any
// program, by whatever name. Windows hands us:
//
//     "<this shim>" "<real explorer path>" <the original arguments>
//
// We look at the original arguments and split two ways:
//   - a folder path, or an Explorer /select /e /root switch naming one:
//     hand it to Davesplorer, which understands those switches.
//   - anything else -- no arguments (that is the desktop shell itself),
//     shell: URIs, ::{CLSID} items, /factory -Embedding COM launches,
//     paths that do not exist: run the REAL Explorer unchanged.
//
// Pass-through is the default and the safe side: the only launches diverted
// are the ones we positively recognize as "browse this folder". The real
// Explorer is run from a private copy under a different name, because
// launching explorer.exe again would just come back here.
//
// Deliberately tiny and dependency-free: it runs on the shell's startup
// path, and a fault here is a fault in the desktop.

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <string>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

namespace
{

const wchar_t* kIfeoKey =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\explorer.exe";

std::wstring RegString(HKEY root, const wchar_t* subkey, const wchar_t* value)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
    wchar_t buf[2048] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    return std::wstring(buf);
}

// The quoted exe out of a `"C:\...\davesplorer.exe" "%1"` shell command.
std::wstring ExeFromCommand(const std::wstring& command)
{
    if (command.empty()) return {};
    if (command.front() == L'"')
        {
        const size_t close = command.find(L'"', 1);
        if (close != std::wstring::npos) return command.substr(1, close - 1);
        return {};
        }
    const size_t space = command.find(L' ');
    return space == std::wstring::npos ? command : command.substr(0, space);
}

std::wstring DavesplorerPath()
{
    // The installer records it here; the folder context-menu verb is the
    // fallback so the shim still works when only the base registration ran.
    std::wstring path = RegString(HKEY_LOCAL_MACHINE, kIfeoKey, L"Davesplorer");
    if (path.empty())
        path = ExeFromCommand(
            RegString(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\Davesplorer\\command", nullptr));
    return path;
}

std::wstring ShimDir()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

// The private copy of the real Explorer, beside the shim. Re-created from
// the system copy if it has gone missing, so a deleted copy self-heals
// rather than leaving the shell unable to start.
std::wstring RealExplorerCopy()
{
    std::wstring copy = ShimDir() + L"\\winexplorer_real.exe";
    if (!PathFileExistsW(copy.c_str()))
        {
        wchar_t win[MAX_PATH] = {};
        GetWindowsDirectoryW(win, MAX_PATH);
        const std::wstring system = std::wstring(win) + L"\\explorer.exe";
        CopyFileW(system.c_str(), copy.c_str(), FALSE);
        }
    return copy;
}

std::wstring Lower(std::wstring s)
{
    for (wchar_t& c : s) c = (wchar_t)towlower(c);
    return s;
}

bool StartsWith(const std::wstring& s, const wchar_t* prefix)
{
    const size_t n = wcslen(prefix);
    return s.size() >= n && Lower(s).compare(0, n, prefix) == 0;
}

// Does this original argument mean "browse a folder"?
bool ArgIsFolder(const std::wstring& arg)
{
    if (arg.empty()) return false;
    // Explorer's own folder switches, in their comma form ("/select,PATH").
    // A bare "/e" or "/n" with no path is a view request for the shell, not
    // a folder to divert.
    if (StartsWith(arg, L"/select,") || StartsWith(arg, L"/e,") || StartsWith(arg, L"/root,"))
        {
        const size_t comma = arg.find(L',');
        std::wstring path = arg.substr(comma + 1);
        if (!path.empty() && path.front() == L'"' && path.back() == L'"') path = path.substr(1, path.size() - 2);
        return PathFileExistsW(path.c_str()) != FALSE;
        }
    // Shell namespace items and COM launches are Explorer's business.
    if (arg[0] == L'/' || arg[0] == L'-') return false;
    if (StartsWith(arg, L"shell:") || StartsWith(arg, L"::{") || arg.find(L"::{") == 0) return false;
    // A real path on disk (folder or file) goes to Davesplorer, which opens
    // the folder and selects a file.
    return PathFileExistsW(arg.c_str()) != FALSE;
}

const wchar_t* SkipToken(const wchar_t* p)
{
    while (*p == L' ' || *p == L'\t') ++p;
    if (*p == L'"')
        {
        ++p;
        while (*p && *p != L'"') ++p;
        if (*p == L'"') ++p;
        }
    else
        {
        while (*p && *p != L' ' && *p != L'\t') ++p;
        }
    return p;
}

void Launch(const std::wstring& exe, const std::wstring& args)
{
    std::wstring cmd = L"\"" + exe + L"\"";
    if (!args.empty()) cmd += L" " + args;
    std::wstring mutableCmd = cmd;   // CreateProcess may write to the buffer
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        }
}

void DryRunLog(const wchar_t* target, const std::wstring& exe, const std::wstring& args)
{
    wchar_t path[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"DSP_SHIM_DRYRUN", path, MAX_PATH) == 0) return;
    std::wstring file = path[0] ? std::wstring(path) : std::wstring();
    if (file.empty() || file == L"1")
        {
        wchar_t tmp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tmp);
        file = std::wstring(tmp) + L"dsp_shim.log";
        }
    HANDLE h = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    std::wstring line = std::wstring(target) + L" | exe=" + exe + L" | args=" + args + L"\r\n";
    const std::string utf8 = [&] {
        const int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s((size_t)(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, s.data(), n, nullptr, nullptr);
        return s;
    }();
    DWORD written = 0;
    WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(h);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    const wchar_t* raw = GetCommandLineW();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(raw, &argc);

    // Windows gives us argv[0] = shim, argv[1] = the real explorer path, and
    // argv[2..] = the original arguments. The original argument STRING is
    // taken verbatim from the raw command line (skip the first two tokens),
    // so quoting is preserved exactly as the caller wrote it.
    const wchar_t* rest = SkipToken(SkipToken(raw));
    while (*rest == L' ' || *rest == L'\t') ++rest;
    const std::wstring origArgs(rest);

    bool browse = false;
    for (int i = 2; i < argc; ++i)
        if (ArgIsFolder(argv[i]))
            {
            browse = true;
            break;
            }
    if (argv) LocalFree(argv);

    const std::wstring davesplorer = DavesplorerPath();
    const std::wstring explorerCopy = RealExplorerCopy();

    if (browse && !davesplorer.empty())
        {
        DryRunLog(L"DAVESPLORER", davesplorer, origArgs);
        if (GetEnvironmentVariableW(L"DSP_SHIM_DRYRUN", nullptr, 0) == 0) Launch(davesplorer, origArgs);
        return 0;
        }

    DryRunLog(L"EXPLORER", explorerCopy, origArgs);
    if (GetEnvironmentVariableW(L"DSP_SHIM_DRYRUN", nullptr, 0) == 0) Launch(explorerCopy, origArgs);
    return 0;
}
