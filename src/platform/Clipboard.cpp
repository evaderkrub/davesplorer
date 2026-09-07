#include "platform/Clipboard.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstring>

namespace platform
{

namespace
{

// Another process may hold the clipboard for a moment; Explorer retries
// too rather than failing on the first attempt.
bool OpenClipboardRetry()
{
    for (int attempt = 0; attempt < 10; ++attempt)
        {
        if (OpenClipboard(nullptr)) return true;
        Sleep(10);
        }
    return false;
}

UINT PreferredDropEffectFormat()
{
    static const UINT fmt = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
    return fmt;
}

} // namespace

bool SetClipboardFiles(const std::vector<std::string>& paths, bool cut, std::string& error)
{
    std::wstring list;
    for (const std::string& p : paths)
        {
        list += Utf8ToWide(p);
        list += L'\0';
        }
    list += L'\0';

    const size_t bytes = sizeof(DROPFILES) + list.size() * sizeof(wchar_t);
    HGLOBAL hDrop = GlobalAlloc(GMEM_MOVEABLE, bytes);
    HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    if (!hDrop || !hEffect)
        {
        if (hDrop) GlobalFree(hDrop);
        if (hEffect) GlobalFree(hEffect);
        error = "Out of memory.";
        return false;
        }
    DROPFILES* df = (DROPFILES*)GlobalLock(hDrop);
    std::memset(df, 0, sizeof(DROPFILES));
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    std::memcpy((char*)df + sizeof(DROPFILES), list.data(), list.size() * sizeof(wchar_t));
    GlobalUnlock(hDrop);

    DWORD* effect = (DWORD*)GlobalLock(hEffect);
    *effect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
    GlobalUnlock(hEffect);

    if (!OpenClipboardRetry())
        {
        GlobalFree(hDrop);
        GlobalFree(hEffect);
        error = "The clipboard is in use by another program.";
        return false;
        }
    EmptyClipboard();
    // The clipboard owns the handles from here on, success or not.
    const bool ok = SetClipboardData(CF_HDROP, hDrop) != nullptr &&
                    SetClipboardData(PreferredDropEffectFormat(), hEffect) != nullptr;
    CloseClipboard();
    if (!ok) error = WinErrorMessage(GetLastError());
    return ok;
}

bool GetClipboardFiles(std::vector<std::string>& paths, bool& cut)
{
    paths.clear();
    cut = false;
    if (!IsClipboardFormatAvailable(CF_HDROP)) return false;
    if (!OpenClipboardRetry()) return false;

    bool ok = false;
    if (HDROP drop = (HDROP)GetClipboardData(CF_HDROP))
        {
        const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
            {
            const UINT len = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring name(len + 1, L'\0');
            DragQueryFileW(drop, i, name.data(), len + 1);
            name.resize(len);
            paths.push_back(WideToUtf8(name));
            }
        ok = !paths.empty();
        }
    if (HGLOBAL hEffect = GetClipboardData(PreferredDropEffectFormat()))
        {
        if (const DWORD* effect = (const DWORD*)GlobalLock(hEffect))
            {
            cut = (*effect & DROPEFFECT_MOVE) != 0;
            GlobalUnlock(hEffect);
            }
        }
    CloseClipboard();
    return ok;
}

bool ClipboardHasFiles()
{
    return IsClipboardFormatAvailable(CF_HDROP) != 0;
}

unsigned long ClipboardSequence()
{
    return GetClipboardSequenceNumber();
}

bool ClearClipboard()
{
    if (!OpenClipboardRetry()) return false;
    EmptyClipboard();
    CloseClipboard();
    return true;
}

} // namespace platform
