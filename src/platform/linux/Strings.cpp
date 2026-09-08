#include "platform/Strings.h"
#include <glib.h>
#include <algorithm>
#include <cwctype>

namespace platform
{
std::wstring Utf8ToWide(std::string_view text)
{
    std::wstring out;
    const char* p = text.data();
    const char* end = p + text.size();
    while (p < end)
        {
        gunichar c = g_utf8_get_char_validated(p, end - p);
        if (c == (gunichar)-1 || c == (gunichar)-2) { out += L'\ufffd'; ++p; }
        else { out += (wchar_t)c; p = g_utf8_next_char(p); }
        }
    return out;
}
std::string WideToUtf8(std::wstring_view text)
{
    std::string out;
    for (wchar_t c : text)
        {
        char buf[6];
        const int n = g_unichar_to_utf8(g_unichar_validate(c) ? c : 0xfffd, buf);
        out.append(buf, n);
        }
    return out;
}
std::string ToLowerAscii(std::string_view text)
{
    std::string out(text);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return out;
}
int NaturalCompare(const std::wstring& a, const std::wstring& b)
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
        {
        if (a[i] >= L'0' && a[i] <= L'9' && b[j] >= L'0' && b[j] <= L'9')
            {
            size_t ae = i, be = j;
            while (ae < a.size() && a[ae] >= L'0' && a[ae] <= L'9') ++ae;
            while (be < b.size() && b[be] >= L'0' && b[be] <= L'9') ++be;
            while (i < ae && a[i] == L'0') ++i;
            while (j < be && b[j] == L'0') ++j;
            if (ae - i != be - j) return ae - i < be - j ? -1 : 1;
            const int cmp = a.compare(i, ae - i, b, j, be - j);
            if (cmp) return cmp;
            i = ae; j = be;
            }
        else
            {
            const auto ac = g_unichar_tolower(a[i++]), bc = g_unichar_tolower(b[j++]);
            if (ac != bc) return ac < bc ? -1 : 1;
            }
        }
    return (i < a.size()) - (j < b.size());
}
bool ContainsNoCase(std::string_view haystack, std::string_view needle)
{
    // Invalid filesystem bytes remain displayable; do not pass them to GLib's Unicode routines.
    const std::string h = WideToUtf8(Utf8ToWide(haystack)), n = WideToUtf8(Utf8ToWide(needle));
    char* hf = g_utf8_casefold(h.c_str(), -1);
    char* nf = g_utf8_casefold(n.c_str(), -1);
    const bool found = std::string(hf).find(nf) != std::string::npos;
    g_free(hf); g_free(nf);
    return found;
}
}
