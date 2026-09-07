#include "platform/Strings.h"

#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <cctype>

namespace platform
{

std::wstring Utf8ToWide(std::string_view utf8)
{
    if (utf8.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out((size_t)needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), out.data(), needed);
    return out;
}

std::string WideToUtf8(std::wstring_view wide)
{
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out((size_t)needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), out.data(), needed, nullptr, nullptr);
    return out;
}

std::string ToLowerAscii(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
        c = (char)std::tolower((unsigned char)c);
    return out;
}

int NaturalCompare(const std::wstring& a, const std::wstring& b)
{
    return StrCmpLogicalW(a.c_str(), b.c_str());
}

bool ContainsNoCase(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) return true;
    // Case folding on the wide form handles non-ASCII letters the way the
    // user expects; a byte-wise tolower would only fold A-Z.
    std::wstring h = Utf8ToWide(haystack);
    std::wstring n = Utf8ToWide(needle);
    CharLowerBuffW(h.data(), (DWORD)h.size());
    CharLowerBuffW(n.data(), (DWORD)n.size());
    return h.find(n) != std::wstring::npos;
}

} // namespace platform
