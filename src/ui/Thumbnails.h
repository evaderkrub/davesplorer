// Thumbnails for the file grid: small textures of image files, decoded on
// a worker thread so a folder of large photos never stalls the interface,
// kept in a bounded cache keyed by path and modification time.
#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>

namespace ui::thumbnails
{

// Longest edge a thumbnail is reduced to. Fixed rather than tied to the
// grid size, so changing the size does not re-decode every file.
inline constexpr int kDim = 256;

struct Thumb
{
    ImTextureData* texture = nullptr;   // null while loading, or when failed
    int  width = 0;                     // of the texture (reduced)
    int  height = 0;
    int  sourceWidth = 0;               // of the file
    int  sourceHeight = 0;
    bool failed = false;
    bool ready() const { return texture != nullptr || failed; }
};

// The thumbnail if it has been decoded; otherwise queues it (most recently
// asked first, since that is what is on screen) and returns null. Calling
// every frame for every visible cell is the intended use: it is what keeps
// an entry alive in the cache.
const Thumb* Get(const std::string& path, int64_t modified);

// Once per frame, inside the frame: turns finished decodes into textures
// and drops the least recently shown entries when the cache is over budget.
void Pump();

// Stops the worker and releases every texture. Before the renderer
// backend shuts down.
void Shutdown();

} // namespace ui::thumbnails
