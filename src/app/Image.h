// Image files the built-in viewer can show: which they are, decoding them
// to RGBA, and finding the other images in the same folder to step through.
#pragma once

#include "app/AppState.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace app
{

// Formats stb_image decodes: png, jpg/jpeg, bmp, gif (first frame), tga,
// psd, hdr, pic, pnm. The extension is lower-case without the dot, as a
// FileEntry carries it.
bool IsViewableImage(std::string_view extension);

struct DecodedImage
{
    int  width = 0;                  // of the pixels below
    int  height = 0;
    int  sourceWidth = 0;            // of the file; larger than the above when reduced
    int  sourceHeight = 0;
    bool hasAlpha = false;           // the file had an alpha channel
    std::vector<uint8_t> rgba;       // width * height * 4
};

// Decodes to 8-bit RGBA. A side longer than maxDim is box-downsampled by an
// integer factor, so a huge photo still fits in one GPU texture; width and
// height report the reduced size. Returns false with a reason on failure.
bool DecodeImageFile(const std::string& path, int maxDim, DecodedImage& out, std::string& error);

// The viewable images in the folder holding `path`, in the list's natural
// name order, so Previous/Next step through them the way Explorer's viewer
// does. Hidden files are included; `path` itself is among them when it is
// an image that still exists.
std::vector<std::string> ImageSiblings(const std::string& path);

// Which of the file views shows this path, or -1.
int FindFileView(const AppState& state, const std::string& path);

// Opens a file view on the path (or focuses the one already showing it via
// the returned index, which the interface treats the same). Returns the
// index into state.fileViews.
int  OpenFileView(AppState& state, const std::string& path);
void CloseFileView(AppState& state, int index);

} // namespace app
