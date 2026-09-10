#include "platform/Strings.h"
#import <Foundation/Foundation.h>
#include <xlocale.h>
#include <cwctype>

namespace platform
{
namespace
{
// The process locale is left alone, so lowering asks for a UTF-8 locale of
// its own: macOS's ctype tables only reach past ASCII in one of those.
locale_t UnicodeLocale()
{
    static locale_t locale = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
    return locale;
}
wchar_t Lower(wchar_t c)
{
    if (locale_t locale = UnicodeLocale()) return (wchar_t)towlower_l((wint_t)c, locale);
    return c >= L'A' && c <= L'Z' ? (wchar_t)(c + (L'a' - L'A')) : c;
}
}
std::wstring Utf8ToWide(std::string_view text)
{
    // Decoded by hand rather than through Foundation: invalid bytes off the
    // filesystem must survive as U+FFFD instead of failing the whole string.
    static const char32_t kLowest[] = {0, 0x80, 0x800, 0x10000};
    std::wstring out;
    const auto* p = reinterpret_cast<const unsigned char*>(text.data());
    const auto* end = p + text.size();
    while (p < end)
        {
        const unsigned char lead = *p;
        int extra;
        char32_t c;
        if (lead < 0x80)             { extra = 0; c = lead; }
        else if ((lead & 0xe0) == 0xc0) { extra = 1; c = lead & 0x1fu; }
        else if ((lead & 0xf0) == 0xe0) { extra = 2; c = lead & 0x0fu; }
        else if ((lead & 0xf8) == 0xf0) { extra = 3; c = lead & 0x07u; }
        else                            { out += L'\ufffd'; ++p; continue; }
        if (end - p <= extra) { out += L'\ufffd'; ++p; continue; }
        bool ok = true;
        for (int i = 1; i <= extra; ++i)
            {
            const unsigned char b = p[i];
            if ((b & 0xc0) != 0x80) { ok = false; break; }
            c = (c << 6) | (b & 0x3fu);
            }
        if (!ok || c < kLowest[extra] || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            { out += L'\ufffd'; ++p; continue; }
        out += (wchar_t)c;
        p += extra + 1;
        }
    return out;
}
std::string WideToUtf8(std::wstring_view text)
{
    std::string out;
    for (wchar_t wide : text)
        {
        char32_t c = (char32_t)wide;
        if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) c = 0xfffd;
        if (c < 0x80) out += (char)c;
        else if (c < 0x800)
            {
            out += (char)(0xc0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3f));
            }
        else if (c < 0x10000)
            {
            out += (char)(0xe0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
            }
        else
            {
            out += (char)(0xf0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3f));
            out += (char)(0x80 | ((c >> 6) & 0x3f));
            out += (char)(0x80 | (c & 0x3f));
            }
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
            const wchar_t ac = Lower(a[i++]), bc = Lower(b[j++]);
            if (ac != bc) return ac < bc ? -1 : 1;
            }
        }
    return (i < a.size()) - (j < b.size());
}
bool ContainsNoCase(std::string_view haystack, std::string_view needle)
{
    // Invalid filesystem bytes remain displayable; Foundation rejects them,
    // so the round trip through UTF-32 sanitizes both sides first.
    @autoreleasepool
        {
        const std::string h = WideToUtf8(Utf8ToWide(haystack)), n = WideToUtf8(Utf8ToWide(needle));
        NSString* text = [NSString stringWithUTF8String:h.c_str()];
        NSString* pattern = [NSString stringWithUTF8String:n.c_str()];
        if (!text || !pattern) return false;
        if (pattern.length == 0) return true;
        return [text rangeOfString:pattern options:NSCaseInsensitiveSearch].location != NSNotFound;
        }
}
}
