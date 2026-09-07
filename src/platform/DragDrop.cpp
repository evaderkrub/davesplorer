#include "platform/DragDrop.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace platform
{

namespace
{

// The minimum an OLE drop source has to say: keep going while the left
// button is down, drop when it goes up, cancel on Escape, and let the shell
// pick the cursors.
class DropSource : public IDropSource
{
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
        if (riid == IID_IUnknown || riid == IID_IDropSource)
            {
            *out = this;
            AddRef();
            return S_OK;
            }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&m_refs); }
    ULONG STDMETHODCALLTYPE Release() override { return (ULONG)InterlockedDecrement(&m_refs); }

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keyState) override
    {
        if (escapePressed) return DRAGDROP_S_CANCEL;
        if (!(keyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }

private:
    LONG m_refs = 1;   // lives on the stack; never deleted through Release
};

} // namespace

bool DragFilesOut(const std::vector<std::string>& paths, DragOutcome& outcome, std::string& error)
{
    outcome = DragOutcome::Cancelled;
    if (paths.empty()) return true;

    // A shell data object built from the items carries every format Explorer
    // and other programs expect (CF_HDROP, shell ID lists, ...), so nothing
    // has to be hand-assembled here.
    std::vector<PIDLIST_ABSOLUTE> pidls;
    for (const std::string& p : paths)
        {
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHParseDisplayName(Utf8ToWide(p).c_str(), nullptr, &pidl, 0, nullptr)) && pidl)
            pidls.push_back(pidl);
        }
    if (pidls.empty())
        {
        error = "None of the dragged items could be found.";
        return false;
        }

    IShellItemArray* items = nullptr;
    IDataObject* data = nullptr;
    HRESULT hr = SHCreateShellItemArrayFromIDLists((UINT)pidls.size(), (PCIDLIST_ABSOLUTE_ARRAY)pidls.data(), &items);
    if (SUCCEEDED(hr)) hr = items->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data));

    bool ok = false;
    if (SUCCEEDED(hr))
        {
        DropSource source;
        DWORD effect = DROPEFFECT_NONE;
        const HRESULT result = DoDragDrop(data, &source, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect);
        ok = true;
        if (result == DRAGDROP_S_DROP)
            {
            if (effect & DROPEFFECT_MOVE)      outcome = DragOutcome::Moved;
            else if (effect & DROPEFFECT_COPY) outcome = DragOutcome::Copied;
            else if (effect & DROPEFFECT_LINK) outcome = DragOutcome::Linked;
            }
        }
    else
        {
        error = "Could not start the drag: " + WinErrorMessage((unsigned long)hr);
        }

    if (data) data->Release();
    if (items) items->Release();
    for (PIDLIST_ABSOLUTE pidl : pidls) ILFree(pidl);
    return ok;
}

} // namespace platform
