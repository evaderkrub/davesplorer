#include "platform/Shell.h"
#include <gio/gio.h>
#include <filesystem>
namespace platform
{
namespace
{
bool Finish(bool ok, GError* e, std::string& error)
{
    error = e ? e->message : "";
    if (e) g_error_free(e);
    return ok;
}
}
bool OpenWithShell(const std::string& path, std::string& error)
{
    GFile* file = g_file_new_for_path(path.c_str());
    char* uri = g_file_get_uri(file);
    GError* e = nullptr;
    const bool ok = g_app_info_launch_default_for_uri(uri, nullptr, &e);
    g_free(uri); g_object_unref(file);
    return Finish(ok, e, error);
}
bool OpenFolderInExplorer(const std::string& folder, std::string& error)
{ return OpenWithShell(folder.empty() ? "/" : folder, error); }
bool ShowInExplorer(const std::string& path, std::string& error)
{
    GError* e = nullptr;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &e);
    if (bus)
        {
        GFile* file = g_file_new_for_path(path.c_str());
        char* uri = g_file_get_uri(file);
        const char* uris[] = {uri, nullptr};
        GVariant* result = g_dbus_connection_call_sync(bus, "org.freedesktop.FileManager1",
            "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1", "ShowItems",
            g_variant_new("(^ass)", uris, ""), nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &e);
        g_free(uri); g_object_unref(file); g_object_unref(bus);
        if (result) { g_variant_unref(result); return true; }
        }
    if (e) g_error_free(e);
    return OpenWithShell(std::filesystem::path(path).parent_path().string(), error);
}
bool OpenTerminalAt(const std::string& dir, std::string& error)
{
    // Launch argv directly: filenames must never be interpreted as shell code.
    for (const char* program : {"xdg-terminal-exec", "x-terminal-emulator", "gnome-terminal", "konsole", "xterm"})
        {
        char* executable = g_find_program_in_path(program);
        if (!executable) continue;
        char* args[] = {executable, nullptr};
        GError* e = nullptr;
        const bool ok = g_spawn_async(dir.c_str(), args, nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, &e);
        g_free(executable);
        return Finish(ok, e, error);
        }
    error = "No supported terminal emulator was found.";
    return false;
}
}
