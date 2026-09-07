// UTF-8 <-> UTF-16 conversion at the Win32 boundary.
//
// Everything above the platform layer speaks UTF-8 (ImGui wants it, and it
// keeps the app code free of wchar_t). Win32 wants UTF-16, so every call into
// the OS converts on the way in and out.
#pragma once

#include <string>
#include <string_view>

namespace platform
{

std::wstring Utf8ToWide(std::string_view utf8);
std::string  WideToUtf8(std::wstring_view wide);

// ASCII-only lowering: extensions and search filters, not display text.
std::string ToLowerAscii(std::string_view text);

// Explorer-style ordering ("file2" before "file10"), via StrCmpLogicalW.
// Takes wide strings so a sort does not re-convert on every comparison.
int NaturalCompare(const std::wstring& a, const std::wstring& b);

// Case-insensitive substring test on UTF-8 text, for the search box.
bool ContainsNoCase(std::string_view haystack, std::string_view needle);

} // namespace platform
