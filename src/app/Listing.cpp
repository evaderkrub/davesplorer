#include "app/Listing.h"
#include "platform/Strings.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace app
{

namespace
{

// Shell type-name lookups go through the registry; one per extension per
// process is plenty.
std::unordered_map<std::string, std::string>& TypeNameCache()
{
    static std::unordered_map<std::string, std::string> cache;
    return cache;
}

std::string TypeNameFor(const platform::FileEntry& e)
{
    if (e.isDirectory) return "File folder";
    auto& cache = TypeNameCache();
    auto it = cache.find(e.extension);
    if (it != cache.end()) return it->second;
    std::string name = platform::FileTypeName(e.extension, false);
    cache.emplace(e.extension, name);
    return name;
}

void LoadThisPC(Tab& tab)
{
    for (const platform::DriveInfo& d : platform::GetDrives())
        {
        platform::FileEntry e;
        e.name        = d.displayName;
        e.nameWide    = platform::Utf8ToWide(d.root);   // sort drives by letter, not label
        e.path        = d.root;
        e.isDirectory = true;
        e.size        = d.totalBytes - d.freeBytes;
        tab.entries.push_back(std::move(e));
        tab.typeNames.push_back(d.typeName);
        }
}

} // namespace

void ReloadIfNeeded(Tab& tab, const Settings& settings)
{
    if (!tab.needsReload) return;
    tab.needsReload = false;

    std::unordered_set<std::string> keep;
    for (size_t i = 0; i < tab.entries.size(); ++i)
        if (i < tab.selected.size() && tab.selected[i]) keep.insert(tab.entries[i].name);
    const std::string focusedName = (tab.focused >= 0 && tab.focused < (int)tab.entries.size())
                                        ? tab.entries[(size_t)tab.focused].name : std::string();

    tab.entries.clear();
    tab.typeNames.clear();
    tab.error.clear();
    if (tab.path.empty())
        {
        LoadThisPC(tab);
        }
    else if (platform::ListDirectory(tab.path, tab.entries, tab.error))
        {
        tab.typeNames.reserve(tab.entries.size());
        for (const platform::FileEntry& e : tab.entries)
            tab.typeNames.push_back(TypeNameFor(e));
        }

    tab.selected.assign(tab.entries.size(), 0);
    tab.focused = -1;
    tab.anchor = -1;
    for (size_t i = 0; i < tab.entries.size(); ++i)
        {
        if (keep.count(tab.entries[i].name)) tab.selected[i] = 1;
        if (!focusedName.empty() && tab.entries[i].name == focusedName) tab.focused = (int)i;
        }
    ++tab.listingGeneration;
    ApplyView(tab, settings);
}

void SortEntries(Tab& tab)
{
    const SortSpec spec = tab.sort;
    const auto& entries = tab.entries;
    const auto& types = tab.typeNames;
    std::stable_sort(tab.visible.begin(), tab.visible.end(), [&](int ia, int ib) {
        const platform::FileEntry& a = entries[(size_t)ia];
        const platform::FileEntry& b = entries[(size_t)ib];
        // Folders stay above files whichever way the column is sorted; that
        // is how Explorer behaves and what users expect.
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        int cmp = 0;
        switch (spec.column)
            {
            case SortColumn::Modified:
                cmp = (a.modified < b.modified) ? -1 : (a.modified > b.modified) ? 1 : 0;
                break;
            case SortColumn::Type:
                cmp = types[(size_t)ia].compare(types[(size_t)ib]);
                break;
            case SortColumn::Size:
                cmp = (a.size < b.size) ? -1 : (a.size > b.size) ? 1 : 0;
                break;
            case SortColumn::Name:
                break;
            }
        if (cmp == 0) cmp = platform::NaturalCompare(a.nameWide, b.nameWide);
        return spec.ascending ? cmp < 0 : cmp > 0;
    });
}

void ApplyView(Tab& tab, const Settings& settings)
{
    tab.visible.clear();
    tab.visible.reserve(tab.entries.size());
    for (size_t i = 0; i < tab.entries.size(); ++i)
        {
        const platform::FileEntry& e = tab.entries[i];
        if (!settings.showHidden && (e.isHidden || e.isSystem)) continue;
        if (!tab.filter.empty() && !platform::ContainsNoCase(e.name, tab.filter)) continue;
        tab.visible.push_back((int)i);
        }
    SortEntries(tab);
}

} // namespace app
