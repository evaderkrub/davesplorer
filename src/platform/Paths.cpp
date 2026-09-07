#include "platform/Paths.h"
#include "platform/Strings.h"

#include <windows.h>

#include <vector>

namespace platform
{

std::string ExecutableDir()
{
    // Grow until the whole path fits; MAX_PATH is not a real limit any more.
    std::vector<wchar_t> buf(512);
    for (;;)
        {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return ".";
        if (n < buf.size() - 1) break;
        buf.resize(buf.size() * 2);
        }
    std::wstring full(buf.data());
    const size_t slash = full.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return ".";
    return WideToUtf8(full.substr(0, slash));
}

} // namespace platform
