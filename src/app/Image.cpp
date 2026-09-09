#include "app/Image.h"
#include "app/PathUtil.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include "stb_image.h"

#include <algorithm>
#include <cstring>

namespace app
{

bool IsViewableImage(std::string_view extension)
{
    static const char* const kViewable[] = { "png", "jpg", "jpeg", "bmp", "gif", "tga", "psd",
                                             "hdr", "pic", "ppm", "pgm", "pnm" };
    for (const char* e : kViewable)
        if (extension == e) return true;
    return false;
}

namespace
{

// Averages factor x factor blocks. The last partial row/column of blocks
// averages whatever pixels it covers, so no edge is dropped.
void BoxDownsample(const uint8_t* src, int w, int h, int factor, DecodedImage& out)
{
    out.width = (w + factor - 1) / factor;
    out.height = (h + factor - 1) / factor;
    out.rgba.assign((size_t)out.width * out.height * 4, 0);
    for (int oy = 0; oy < out.height; ++oy)
        for (int ox = 0; ox < out.width; ++ox)
            {
            const int x0 = ox * factor, y0 = oy * factor;
            const int x1 = std::min(x0 + factor, w), y1 = std::min(y0 + factor, h);
            uint32_t sum[4] = { 0, 0, 0, 0 };
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    {
                    const uint8_t* p = src + ((size_t)y * w + x) * 4;
                    for (int c = 0; c < 4; ++c) sum[c] += p[c];
                    }
            const uint32_t n = (uint32_t)((x1 - x0) * (y1 - y0));
            uint8_t* o = out.rgba.data() + ((size_t)oy * out.width + ox) * 4;
            for (int c = 0; c < 4; ++c) o[c] = (uint8_t)((sum[c] + n / 2) / n);
            }
}

} // namespace

bool DecodeImageFile(const std::string& path, int maxDim, DecodedImage& out, std::string& error)
{
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!pixels)
        {
        const char* reason = stbi_failure_reason();
        error = "Could not decode " + PathName(path) + (reason ? std::string(": ") + reason : std::string());
        return false;
        }
    out.hasAlpha = channels == 2 || channels == 4;
    out.sourceWidth = w;
    out.sourceHeight = h;
    const int longest = std::max(w, h);
    if (maxDim > 0 && longest > maxDim)
        {
        BoxDownsample(pixels, w, h, (longest + maxDim - 1) / maxDim, out);
        }
    else
        {
        out.width = w;
        out.height = h;
        out.rgba.assign(pixels, pixels + (size_t)w * h * 4);
        }
    stbi_image_free(pixels);
    return true;
}

std::vector<std::string> ImageSiblings(const std::string& path)
{
    std::vector<platform::FileEntry> entries;
    std::string error;
    std::vector<std::string> result;
    if (!platform::ListDirectory(ParentPath(path), entries, error)) return result;
    std::vector<const platform::FileEntry*> images;
    for (const platform::FileEntry& e : entries)
        if (!e.isDirectory && IsViewableImage(e.extension)) images.push_back(&e);
    std::sort(images.begin(), images.end(), [](const platform::FileEntry* a, const platform::FileEntry* b) {
        return platform::NaturalCompare(a->nameWide, b->nameWide) < 0;
    });
    for (const platform::FileEntry* e : images) result.push_back(e->path);
    return result;
}

int FindFileView(const AppState& state, const std::string& path)
{
    for (size_t i = 0; i < state.fileViews.size(); ++i)
        if (state.fileViews[i].path == path) return (int)i;
    return -1;
}

int OpenFileView(AppState& state, const std::string& path)
{
    const int existing = FindFileView(state, path);
    if (existing >= 0) return existing;
    FileView view;
    view.id = state.nextTabId++;   // shared with folder views so every window id is distinct
    view.path = path;
    state.fileViews.push_back(view);
    return (int)state.fileViews.size() - 1;
}

void CloseFileView(AppState& state, int index)
{
    if (index < 0 || index >= (int)state.fileViews.size()) return;
    state.fileViews.erase(state.fileViews.begin() + index);
}

} // namespace app
