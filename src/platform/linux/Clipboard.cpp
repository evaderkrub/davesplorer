#include "platform/Clipboard.h"
#include <SDL3/SDL.h>
#include <glib.h>
#include <sstream>
#include <cstring>

namespace platform
{
namespace
{
constexpr const char* kFiles = "x-special/gnome-copied-files";
constexpr const char* kUris = "text/uri-list";
unsigned long sequence = 0;
struct Data { std::string files, uris, kde; };
const void* SDLCALL Provide(void* userdata, const char* mime, size_t* size)
{
    if (!mime) { *size = 0; return nullptr; }
    const auto& d = *static_cast<Data*>(userdata);
    const auto& value = std::strcmp(mime, kFiles) == 0 ? d.files :
                        std::strcmp(mime, "application/x-kde-cutselection") == 0 ? d.kde : d.uris;
    *size = value.size();
    return value.data();
}
void SDLCALL Cleanup(void* userdata) { delete static_cast<Data*>(userdata); ++sequence; }
std::string Read(const char* mime)
{
    if (!SDL_HasClipboardData(mime)) return "";
    size_t size = 0;
    void* data = SDL_GetClipboardData(mime, &size);
    if (!data) return "";
    std::string result(static_cast<char*>(data), size);
    SDL_free(data);
    return result;
}
}
bool SetClipboardFiles(const std::vector<std::string>& paths, bool cut, std::string& error)
{
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO))
        { error = "The clipboard requires SDL video initialization."; return false; }
    auto* data = new Data;
    data->files = cut ? "cut\n" : "copy\n";
    data->kde = cut ? "1" : "0";
    for (const auto& path : paths)
        {
        char* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
        if (!uri) { delete data; error = "Invalid clipboard file path."; return false; }
        data->files += std::string(uri) + "\n";
        data->uris += std::string(uri) + "\r\n";
        g_free(uri);
        }
    const char* types[] = {kFiles, kUris, "application/x-kde-cutselection"};
    if (!SDL_SetClipboardData(Provide, Cleanup, data, types, 3))
        { error = SDL_GetError(); SDL_ClearClipboardData(); return false; }
    ++sequence; error.clear(); return true;
}
bool GetClipboardFiles(std::vector<std::string>& paths, bool& cut)
{
    paths.clear(); cut = false;
    std::string value = Read(kFiles);
    if (!value.empty())
        {
        const auto line = value.find('\n');
        if (line == std::string::npos) return false;
        cut = value.substr(0, line) == "cut";
        value.erase(0, line + 1);
        }
    else { value = Read(kUris); cut = Read("application/x-kde-cutselection") == "1"; }
    std::istringstream lines(value);
    std::string line;
    while (std::getline(lines, line))
        {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\0')) line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        char* host = nullptr;
        char* path = g_filename_from_uri(line.c_str(), &host, nullptr);
        if (path && (!host || !*host || std::strcmp(host, "localhost") == 0)) paths.emplace_back(path);
        g_free(path); g_free(host);
        }
    return !paths.empty();
}
bool ClipboardHasFiles() { return SDL_HasClipboardData(kFiles) || SDL_HasClipboardData(kUris); }
unsigned long ClipboardSequence() { return sequence; }
bool ClearClipboard() { const bool ok = SDL_ClearClipboardData(); if (ok) ++sequence; return ok; }
}
