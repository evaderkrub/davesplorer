// Console unit tests for the application layer. No window, no ImGui.
// Exit code is the number of failed checks.

#include "app/AppState.h"
#include "app/FileOps.h"
#include "app/Format.h"
#include "app/Listing.h"
#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "app/Selection.h"
#include "app/Settings.h"
#include "app/Shortcuts.h"
#include "platform/Clipboard.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                                  \
    do                                                                                               \
        {                                                                                            \
        ++g_checks;                                                                                  \
        if (!(cond))                                                                                 \
            {                                                                                        \
            ++g_failures;                                                                            \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                              \
            }                                                                                        \
        }                                                                                            \
    while (0)

#define CHECK_EQ(a, b)                                                                               \
    do                                                                                               \
        {                                                                                            \
        ++g_checks;                                                                                  \
        if (!((a) == (b)))                                                                           \
            {                                                                                        \
            ++g_failures;                                                                            \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);                       \
            }                                                                                        \
        }                                                                                            \
    while (0)

// A scratch folder under %TEMP% that is removed when the test ends.
struct TempDir
{
    std::string path;
    TempDir(const char* tag)
    {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        path = platform::WideToUtf8(tmp) + "davesplorer_" + tag + "_" + std::to_string(GetCurrentProcessId());
        std::string error;
        platform::CreateFolder(path, error);
    }
    ~TempDir()
    {
        std::string error;
        platform::DeletePermanently({ path }, error);
    }
    std::string file(const char* name, const char* content = "x")
    {
        const std::string p = app::JoinPath(path, name);
        std::ofstream(platform::Utf8ToWide(p)) << content;
        return p;
    }
    std::string dir(const char* name)
    {
        const std::string p = app::JoinPath(path, name);
        std::string error;
        platform::CreateFolder(p, error);
        return p;
    }
};

// The clipboard tests use the real clipboard. Whatever text was there is put
// back afterwards; anything else is lost, which is the price of testing the
// real thing.
struct ClipboardKeeper
{
    std::wstring text;
    bool hadText = false;
    ClipboardKeeper()
    {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(nullptr)) return;
        if (HGLOBAL h = GetClipboardData(CF_UNICODETEXT))
            if (const wchar_t* p = (const wchar_t*)GlobalLock(h))
                {
                text = p;
                hadText = true;
                GlobalUnlock(h);
                }
        CloseClipboard();
    }
    ~ClipboardKeeper()
    {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        if (hadText)
            {
            const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
            if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes))
                {
                memcpy(GlobalLock(h), text.c_str(), bytes);
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
                }
            }
        CloseClipboard();
    }
};

void TestPathUtil()
{
    using namespace app;
    CHECK_EQ(NormalizePath("c:"), "C:\\");
    CHECK_EQ(NormalizePath("c:/users/dave/"), "C:\\users\\dave");
    CHECK_EQ(NormalizePath("  C:\\x\\\\y\\ "), "C:\\x\\y");
    CHECK_EQ(NormalizePath("C:\\"), "C:\\");
    CHECK_EQ(NormalizePath(""), "");

    CHECK_EQ(ParentPath("C:\\Users\\dave"), "C:\\Users");
    CHECK_EQ(ParentPath("C:\\Users"), "C:\\");
    CHECK_EQ(ParentPath("C:\\"), "");
    CHECK_EQ(ParentPath(""), "");

    CHECK_EQ(PathName("C:\\Users\\dave"), "dave");
    CHECK_EQ(PathName("C:\\"), "C:\\");
    CHECK_EQ(PathName(""), "This PC");

    CHECK_EQ(JoinPath("C:\\", "x"), "C:\\x");
    CHECK_EQ(JoinPath("C:\\a", "x"), "C:\\a\\x");
    CHECK_EQ(JoinPath("", "x"), "x");

    CHECK(IsDriveRoot("D:\\"));
    CHECK(!IsDriveRoot("D:\\x"));

    const std::vector<Crumb> crumbs = Breadcrumbs("C:\\Users\\dave");
    CHECK_EQ(crumbs.size(), (size_t)4);
    CHECK_EQ(crumbs[0].label, "This PC");
    CHECK_EQ(crumbs[0].path, "");
    CHECK_EQ(crumbs[1].label, "C:");
    CHECK_EQ(crumbs[1].path, "C:\\");
    CHECK_EQ(crumbs[2].path, "C:\\Users");
    CHECK_EQ(crumbs[3].path, "C:\\Users\\dave");
    CHECK_EQ(Breadcrumbs("").size(), (size_t)1);

    const std::vector<Crumb> unc = Breadcrumbs("\\\\server\\share\\folder");
    CHECK_EQ(unc.size(), (size_t)3);
    CHECK_EQ(unc[1].path, "\\\\server\\share");
    CHECK_EQ(unc[2].path, "\\\\server\\share\\folder");

    std::string reason;
    CHECK(IsValidFileName("hello.txt", reason));
    CHECK(!IsValidFileName("", reason));
    CHECK(!IsValidFileName("a/b", reason));
    CHECK(!IsValidFileName("a:b", reason));
    CHECK(!IsValidFileName("con", reason));
    CHECK(!IsValidFileName("COM1.txt", reason));
    CHECK(!IsValidFileName("trailing.", reason));
    CHECK(!IsValidFileName("..", reason));
}

void TestFormat()
{
    using namespace app;
    CHECK_EQ(WithThousands(0), "0");
    CHECK_EQ(WithThousands(999), "999");
    CHECK_EQ(WithThousands(1000), "1,000");
    CHECK_EQ(WithThousands(1234567), "1,234,567");

    CHECK_EQ(SizeInKB(0), "0 KB");
    CHECK_EQ(SizeInKB(1), "1 KB");
    CHECK_EQ(SizeInKB(1024), "1 KB");
    CHECK_EQ(SizeInKB(1025), "2 KB");
    CHECK_EQ(SizeInKB(2048 * 1024), "2,048 KB");

    CHECK_EQ(HumanSize(0), "0 bytes");
    CHECK_EQ(HumanSize(1), "1 byte");
    CHECK_EQ(HumanSize(512), "512 bytes");
    CHECK_EQ(HumanSize(1536), "1.50 KB");
    CHECK_EQ(HumanSize(15 * 1024 * 1024 + 300 * 1024), "15.3 MB");
    CHECK_EQ(HumanSize(200ull * 1024 * 1024 * 1024), "200 GB");
}

void TestSettings()
{
    app::Settings s;
    s.themeIndex = 2;
    s.uiScale = 1.3f;
    s.showHidden = true;
    s.startPath = "C:\\Users";
    s.windowWidth = 1000;

    app::Settings back;
    const std::string text = app::SerializeSettings(s);
    size_t pos = 0;
    while (pos < text.size())
        {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl - pos);
        const size_t eq = line.find('=');
        if (eq != std::string::npos) app::ApplySettingLine(back, line.substr(0, eq), line.substr(eq + 1));
        pos = nl + 1;
        }
    CHECK_EQ(back.themeIndex, 2);
    CHECK(back.uiScale > 1.29f && back.uiScale < 1.31f);
    CHECK(back.showHidden);
    CHECK_EQ(back.startPath, "C:\\Users");
    CHECK_EQ(back.windowWidth, 1000);

    // Garbage in the file must not produce an unusable interface.
    app::ApplySettingLine(back, "ui_scale", "0");
    CHECK_EQ(back.uiScale, 1.0f);
    app::ApplySettingLine(back, "ui_scale", "abc");
    CHECK_EQ(back.uiScale, 1.0f);
    app::ApplySettingLine(back, "window_width", "10");
    CHECK_EQ(back.windowWidth, 400);
}

void TestNavigationAndListing()
{
    TempDir tmp("nav");
    tmp.file("alpha.txt", "aaaa");
    tmp.file("Beta.md", "bb");
    tmp.file("gamma.png", "ggggggg");
    tmp.file("file10.txt");
    tmp.file("file2.txt");
    tmp.dir("sub");
    tmp.dir("Another");

    app::Settings settings;
    app::Tab tab;
    std::string error;
    CHECK(app::NavigateTo(tab, tmp.path, error));
    CHECK_EQ(tab.path, tmp.path);
    CHECK(!app::CanGoBack(tab));
    CHECK(app::CanGoUp(tab));

    app::ReloadIfNeeded(tab, settings);
    CHECK_EQ(tab.entries.size(), (size_t)7);
    CHECK_EQ(tab.visible.size(), (size_t)7);
    // Folders first, natural order within each group.
    CHECK(tab.entries[(size_t)tab.visible[0]].isDirectory);
    CHECK(tab.entries[(size_t)tab.visible[1]].isDirectory);
    CHECK_EQ(tab.entries[(size_t)tab.visible[0]].name, "Another");
    CHECK_EQ(tab.entries[(size_t)tab.visible[1]].name, "sub");
    CHECK_EQ(tab.entries[(size_t)tab.visible[2]].name, "alpha.txt");
    CHECK_EQ(tab.entries[(size_t)tab.visible[3]].name, "Beta.md");
    CHECK_EQ(tab.entries[(size_t)tab.visible[4]].name, "file2.txt");
    CHECK_EQ(tab.entries[(size_t)tab.visible[5]].name, "file10.txt");
    CHECK_EQ(tab.entries[(size_t)tab.visible[6]].name, "gamma.png");

    // Type names come from the shell; folders are always "File folder".
    CHECK_EQ(tab.typeNames[(size_t)tab.visible[0]], "File folder");
    CHECK(!tab.typeNames[(size_t)tab.visible[2]].empty());

    // Sort by size descending: folders still first, then biggest file.
    tab.sort.column = app::SortColumn::Size;
    tab.sort.ascending = false;
    app::ApplyView(tab, settings);
    CHECK(tab.entries[(size_t)tab.visible[0]].isDirectory);
    CHECK_EQ(tab.entries[(size_t)tab.visible[2]].name, "gamma.png");

    // Filter is case-insensitive.
    tab.sort = app::SortSpec{};
    tab.filter = "BETA";
    app::ApplyView(tab, settings);
    CHECK_EQ(tab.visible.size(), (size_t)1);
    CHECK_EQ(tab.entries[(size_t)tab.visible[0]].name, "Beta.md");
    tab.filter.clear();
    app::ApplyView(tab, settings);

    // Navigate into a subfolder and back.
    const std::string sub = app::JoinPath(tmp.path, "sub");
    CHECK(app::NavigateTo(tab, sub, error));
    CHECK(app::CanGoBack(tab));
    app::ReloadIfNeeded(tab, settings);
    CHECK_EQ(tab.entries.size(), (size_t)0);
    app::GoBack(tab);
    CHECK_EQ(tab.path, tmp.path);
    CHECK(app::CanGoForward(tab));
    app::GoForward(tab);
    CHECK_EQ(tab.path, sub);
    app::GoUp(tab);
    CHECK_EQ(tab.path, tmp.path);
    // Going somewhere new discards the forward branch.
    CHECK(!app::CanGoForward(tab));

    // Bad paths fail with a message and leave the tab alone.
    CHECK(!app::NavigateTo(tab, app::JoinPath(tmp.path, "nope"), error));
    CHECK(!error.empty());
    CHECK_EQ(tab.path, tmp.path);
    CHECK(!app::NavigateTo(tab, app::JoinPath(tmp.path, "alpha.txt"), error));

    // This PC lists drives.
    CHECK(app::NavigateTo(tab, "", error));
    app::ReloadIfNeeded(tab, settings);
    CHECK(!tab.entries.empty());
    CHECK(tab.entries[0].isDirectory);
    CHECK(!app::CanGoUp(tab));

    // Hidden files are excluded unless asked for.
    const std::string hidden = tmp.file("hidden.txt");
    SetFileAttributesW(platform::Utf8ToWide(hidden).c_str(), FILE_ATTRIBUTE_HIDDEN);
    CHECK(app::NavigateTo(tab, tmp.path, error));
    app::ReloadIfNeeded(tab, settings);
    CHECK_EQ(tab.entries.size(), (size_t)8);
    CHECK_EQ(tab.visible.size(), (size_t)7);
    settings.showHidden = true;
    app::ApplyView(tab, settings);
    CHECK_EQ(tab.visible.size(), (size_t)8);
}

void TestSelection()
{
    TempDir tmp("sel");
    for (int i = 0; i < 6; ++i) tmp.file(("f" + std::to_string(i) + ".txt").c_str());
    app::Settings settings;
    app::Tab tab;
    std::string error;
    CHECK(app::NavigateTo(tab, tmp.path, error));
    app::ReloadIfNeeded(tab, settings);
    CHECK_EQ(tab.visible.size(), (size_t)6);

    app::ClickSelect(tab, 1, false, false);
    CHECK_EQ(app::SelectedCount(tab), 1);
    CHECK_EQ(tab.focused, tab.visible[1]);
    app::ClickSelect(tab, 3, true, false);
    CHECK_EQ(app::SelectedCount(tab), 2);
    app::ClickSelect(tab, 3, true, false);   // ctrl again toggles off, anchor stays at 3
    CHECK_EQ(app::SelectedCount(tab), 1);
    app::ClickSelect(tab, 5, false, true);   // shift from anchor 3 to 5
    CHECK_EQ(app::SelectedCount(tab), 3);
    app::ClickSelect(tab, 1, false, false);
    app::ClickSelect(tab, 4, false, true);   // shift from anchor 1 to 4
    CHECK_EQ(app::SelectedCount(tab), 4);
    app::ClickSelect(tab, 0, false, false);
    CHECK_EQ(app::SelectedCount(tab), 1);

    app::MoveFocus(tab, 1, false, false, false);
    CHECK_EQ(tab.focused, tab.visible[1]);
    CHECK_EQ(app::SelectedCount(tab), 1);
    app::MoveFocus(tab, 1, false, true, false);
    CHECK_EQ(app::SelectedCount(tab), 2);
    app::MoveFocus(tab, 1, true, false, false);
    CHECK_EQ(tab.focused, tab.visible[5]);
    app::MoveFocus(tab, 1, false, false, false);   // clamps at the end
    CHECK_EQ(tab.focused, tab.visible[5]);

    app::SelectAll(tab);
    CHECK_EQ(app::SelectedCount(tab), 6);
    CHECK_EQ(app::SelectedPaths(tab).size(), (size_t)6);
    app::InvertSelection(tab);
    CHECK_EQ(app::SelectedCount(tab), 0);
    app::SelectAll(tab);
    CHECK_EQ(app::SelectedBytes(tab), (uint64_t)6);

    // A selection survives a reload of the same folder.
    app::ClearSelection(tab);
    app::ClickSelect(tab, 2, false, false);
    app::Refresh(tab);
    app::ReloadIfNeeded(tab, settings);
    CHECK_EQ(app::SelectedCount(tab), 1);
    CHECK_EQ(tab.entries[(size_t)tab.visible[2]].name, "f2.txt");
    CHECK(tab.selected[(size_t)tab.visible[2]]);
}

void TestFileOps()
{
    TempDir tmp("ops");
    tmp.file("a.txt", "hello");
    app::AppState state;
    state.tabs.emplace_back();
    app::Tab& tab = state.active();
    std::string error;
    CHECK(app::NavigateTo(tab, tmp.path, error));
    app::ReloadIfNeeded(tab, state.settings);

    CHECK_EQ(app::UniqueNewName(tab, "New folder"), "New folder");
    CHECK(app::CreateFolderIn(tab, "New folder", error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK_EQ(app::UniqueNewName(tab, "New folder"), "New folder (2)");
    CHECK_EQ(app::UniqueNewName(tab, "a.txt"), "a (2).txt");
    CHECK(!app::CreateFolderIn(tab, "bad:name", error));
    CHECK(!error.empty());

    CHECK(app::CreateFileIn(tab, "b.txt", error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(platform::PathExists(app::JoinPath(tmp.path, "b.txt")));

    int idx = -1;
    for (size_t i = 0; i < tab.entries.size(); ++i)
        if (tab.entries[i].name == "a.txt") idx = (int)i;
    CHECK(idx >= 0);
    CHECK(app::RenameEntry(tab, idx, "renamed.txt", error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(platform::PathExists(app::JoinPath(tmp.path, "renamed.txt")));
    CHECK(!platform::PathExists(app::JoinPath(tmp.path, "a.txt")));

    // Copy within the same folder makes " - Copy". This goes through the
    // real Windows clipboard, whose previous contents are put back after.
    ClipboardKeeper keeper;
    app::ClearSelection(tab);
    for (size_t i = 0; i < tab.entries.size(); ++i)
        if (tab.entries[i].name == "renamed.txt") tab.selected[i] = 1;
    CHECK(app::CopySelection(state, tab, false, error));
    CHECK(app::CanPaste(state, tab));
    CHECK(state.clipboard.paths.size() == 1 && !state.clipboard.cut);
    CHECK(app::Paste(state, tab, error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(platform::PathExists(app::JoinPath(tmp.path, "renamed - Copy.txt")));

    // Cut and paste into the subfolder moves the file and empties the clipboard.
    app::ClearSelection(tab);
    for (size_t i = 0; i < tab.entries.size(); ++i)
        if (tab.entries[i].name == "renamed - Copy.txt") tab.selected[i] = 1;
    CHECK(app::CopySelection(state, tab, true, error));
    CHECK(state.clipboard.cut);
    CHECK(app::NavigateTo(tab, app::JoinPath(tmp.path, "New folder"), error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(app::Paste(state, tab, error));
    CHECK(!app::CanPaste(state, tab));
    CHECK(!platform::ClipboardHasFiles());
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(platform::PathExists(app::JoinPath(tab.path, "renamed - Copy.txt")));
    CHECK(!platform::PathExists(app::JoinPath(tmp.path, "renamed - Copy.txt")));

    // A file list placed on the clipboard by another program pastes too.
    CHECK(platform::SetClipboardFiles({ app::JoinPath(tmp.path, "renamed.txt") }, false, error));
    app::TickAppState(state);
    CHECK(state.clipboard.paths.empty());   // not ours, so nothing is ghosted
    CHECK(app::CanPaste(state, tab));
    CHECK(app::Paste(state, tab, error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK(platform::PathExists(app::JoinPath(tab.path, "renamed.txt")));
    CHECK(platform::PathExists(app::JoinPath(tmp.path, "renamed.txt")));

    // Permanent delete of the selection.
    app::SelectAll(tab);
    CHECK(app::DeleteSelected(tab, true, error));
    app::ReloadIfNeeded(tab, state.settings);
    CHECK_EQ(tab.entries.size(), (size_t)0);

    // Nothing can be created or deleted in This PC.
    CHECK(app::NavigateTo(tab, "", error));
    CHECK(!app::CreateFolderIn(tab, "x", error));
    CHECK(!app::DeleteSelected(tab, true, error));
}

void TestClipboardAndDrop()
{
    ClipboardKeeper keeper;
    TempDir tmp("drop");
    const std::string a = tmp.file("a.txt", "aaa");
    const std::string b = tmp.file("b.txt", "bb");
    const std::string sub = tmp.dir("sub");
    const std::string deep = app::JoinPath(sub, "deep");
    std::string error;
    platform::CreateFolder(deep, error);

    // Clipboard round trip with the cut flag.
    std::vector<std::string> back;
    bool cut = true;
    CHECK(platform::SetClipboardFiles({ a, b }, false, error));
    CHECK(platform::ClipboardHasFiles());
    CHECK(platform::GetClipboardFiles(back, cut));
    CHECK_EQ(back.size(), (size_t)2);
    CHECK_EQ(back[0], a);
    CHECK(!cut);
    CHECK(platform::SetClipboardFiles({ b }, true, error));
    CHECK(platform::GetClipboardFiles(back, cut));
    CHECK_EQ(back.size(), (size_t)1);
    CHECK(cut);
    CHECK(platform::ClearClipboard());
    CHECK(!platform::ClipboardHasFiles());
    CHECK(!platform::GetClipboardFiles(back, cut));

    // Explorer's drop rule.
    CHECK(app::DefaultDropAction({ a }, sub, false, false) == app::DropAction::Move);
    CHECK(app::DefaultDropAction({ a }, sub, true, false) == app::DropAction::Copy);
    CHECK(app::DefaultDropAction({ a }, "Z:\\elsewhere", false, false) == app::DropAction::Copy);
    CHECK(app::DefaultDropAction({ a }, "Z:\\elsewhere", false, true) == app::DropAction::Move);

    CHECK(app::CanDropOn({ a }, sub));
    CHECK(!app::CanDropOn({ sub }, sub));
    CHECK(!app::CanDropOn({ sub }, deep));       // a folder into its own subtree
    CHECK(app::CanDropOn({ a }, tmp.path));      // same folder is allowed (copy makes " - Copy")
    CHECK(!app::CanDropOn({ a }, ""));

    app::AppState state;
    state.tabs.emplace_back();
    CHECK(app::NavigateTo(state.tabs[0], tmp.path, error));
    app::TickAppState(state);

    CHECK(app::DropPaths(state, { a }, sub, app::DropAction::Move, error));
    CHECK(platform::PathExists(app::JoinPath(sub, "a.txt")));
    CHECK(!platform::PathExists(a));
    CHECK(state.tabs[0].needsReload);

    CHECK(app::DropPaths(state, { b }, sub, app::DropAction::Copy, error));
    CHECK(platform::PathExists(app::JoinPath(sub, "b.txt")));
    CHECK(platform::PathExists(b));

    // Moving onto the folder the file is in is a quiet no-op.
    CHECK(app::DropPaths(state, { b }, tmp.path, app::DropAction::Move, error));
    CHECK(platform::PathExists(b));

    CHECK(!app::DropPaths(state, { sub }, deep, app::DropAction::Move, error));
    CHECK(!error.empty());
    CHECK(!app::DropPaths(state, { b }, "", app::DropAction::Copy, error));
}

void TestShortcuts()
{
    TempDir tmp("shortcuts");
    const std::string sub = tmp.dir("sub");
    tmp.file("a.txt");

    app::AppState state;
    app::InitAppState(state, tmp.path);
    app::Tab& tab = state.active();
    std::string error;
    CHECK(app::NavigateTo(tab, tmp.path, error));
    app::ReloadIfNeeded(tab, state.settings);

    // Nothing selected: the folder being shown. One folder selected: that
    // folder. A file selected: back to the folder being shown.
    CHECK_EQ(app::ShortcutCandidate(tab), tmp.path);
    for (size_t i = 0; i < tab.entries.size(); ++i)
        if (tab.entries[i].name == "sub") app::ClickSelect(tab, app::VisiblePosOf(tab, (int)i), false, false);
    CHECK_EQ(app::ShortcutCandidate(tab), sub);
    app::SelectAll(tab);
    CHECK_EQ(app::ShortcutCandidate(tab), tmp.path);

    CHECK(!app::ShortcutAssigned(state, 0));
    CHECK(!app::ShortcutAssigned(state, 99));
    app::AssignShortcut(state, 3, sub);
    CHECK(app::ShortcutAssigned(state, 3));
    CHECK_EQ(app::ShortcutPath(state, 3), sub);
    app::AssignShortcut(state, 99, sub);   // out of range is ignored
    app::AssignShortcut(state, 4, "");     // empty is ignored
    CHECK(!app::ShortcutAssigned(state, 4));

    // Assigning writes settings.ini right away; the slot survives a reload.
    app::Settings loaded;
    CHECK(app::LoadSettings(state.settingsFile, loaded));
    CHECK_EQ(loaded.shortcuts.size(), (size_t)app::kShortcutSlots);
    CHECK_EQ(loaded.shortcuts[3], sub);
    CHECK(loaded.shortcuts[0].empty());

    app::ClearShortcut(state, 3);
    CHECK(!app::ShortcutAssigned(state, 3));
    app::Settings reloaded;   // a load overlays the file onto defaults, so start fresh
    CHECK(app::LoadSettings(state.settingsFile, reloaded));
    CHECK(reloaded.shortcuts[3].empty());
}

void TestAppState()
{
    TempDir tmp("state");
    app::AppState state;
    app::InitAppState(state, tmp.path);
    CHECK_EQ(state.tabs.size(), (size_t)1);
    CHECK(!state.drives.empty());
    CHECK(!state.quickAccess.empty());
    CHECK_EQ(state.settingsFile, app::JoinPath(tmp.path, "settings.ini"));

    const int second = app::OpenTab(state, "");
    CHECK_EQ(second, 1);
    CHECK_EQ(state.activeTab, 1);
    CHECK(state.tabs[0].id != state.tabs[1].id);
    app::CloseTab(state, 0);
    CHECK_EQ(state.tabs.size(), (size_t)1);
    CHECK_EQ(state.activeTab, 0);
    CHECK(!state.quitRequested);
    app::CloseTab(state, 0);
    CHECK(state.quitRequested);   // last tab closing means exit, like Explorer

    state.settings.startPath = "";
    app::SaveAppState(state);
    CHECK(platform::PathExists(state.settingsFile));
    app::Settings loaded;
    CHECK(app::LoadSettings(state.settingsFile, loaded));
    CHECK_EQ(loaded.startPath, state.active().path);

    CHECK_EQ(app::LocationTitle(state, ""), "This PC");
    CHECK_EQ(app::LocationTitle(state, state.drives[0].root), state.drives[0].displayName);
}

} // namespace

int main()
{
    TestPathUtil();
    TestFormat();
    TestSettings();
    TestNavigationAndListing();
    TestSelection();
    TestFileOps();
    TestClipboardAndDrop();
    TestShortcuts();
    TestAppState();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}
