// One open location: its listing, what is selected, and where it has been.
#pragma once

#include "platform/FileSystem.h"

#include <cstdint>
#include <string>
#include <vector>

namespace app
{

enum class SortColumn
{
    Name = 0,
    Modified,
    Type,
    Size,
};

struct SortSpec
{
    SortColumn column = SortColumn::Name;
    bool ascending = true;
};

struct Tab
{
    int id = 0;                               // stable identity for the interface; indices shift as tabs close
    std::string path;                         // "" is This PC

    std::vector<platform::FileEntry> entries; // everything the OS reported
    std::vector<std::string> typeNames;       // parallel to entries, shell type names
    std::vector<int> visible;                 // indices into entries after filter and sort
    std::vector<uint8_t> selected;            // parallel to entries
    int focused = -1;                         // entry index with keyboard focus, -1 none
    int anchor = -1;                          // where a shift-range starts

    std::vector<std::string> history;
    int historyIndex = -1;

    std::string filter;                       // search box text
    SortSpec sort;
    std::string error;                        // last listing failure, shown in place of the list
    bool needsReload = true;
    std::string selectOnLoad;                 // entry to select once the next listing arrives

    // Rises with every reload so the interface can notice a new listing
    // (scroll to top, forget stale row state) without comparing vectors.
    uint64_t listingGeneration = 0;
};

} // namespace app
