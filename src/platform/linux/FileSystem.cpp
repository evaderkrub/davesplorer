#include "platform/FileSystem.h"
#include "platform/Strings.h"
#include <gio/gio.h>
#include <filesystem>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/fs.h>

namespace platform
{
namespace fs = std::filesystem;
namespace
{
bool Result(const std::error_code& ec, std::string& error)
{
    error = ec ? ec.message() : "";
    return !ec;
}
bool GResult(bool ok, GError* e, std::string& error)
{
    error = e ? e->message : "";
    if (e) g_error_free(e);
    return ok;
}
bool Transfer(const std::vector<std::string>& sources, const std::string& destDir,
              bool move, bool unique, std::string& error)
{
    if (!IsDirectory(destDir)) { error = "The destination is not a folder."; return false; }
    for (const auto& source : sources)
        {
        std::error_code ec;
        const fs::path src(source);
        fs::path dest = fs::path(destDir) / src.filename();
        const auto canonicalSrc = fs::weakly_canonical(src, ec);
        if (!Result(ec, error)) return false;
        const auto canonicalDest = fs::weakly_canonical(destDir, ec);
        if (!Result(ec, error)) return false;
        const auto relative = canonicalDest.lexically_relative(canonicalSrc);
        if (IsDirectory(source) && (relative.empty() || *relative.begin() != ".."))
            { error = "A folder cannot be copied or moved into itself."; return false; }
        if (unique)
            {
            for (int n = 1; fs::exists(fs::symlink_status(dest, ec)); ++n)
                dest = fs::path(destDir) / (src.stem().string() + " - Copy" +
                    (n == 1 ? "" : " (" + std::to_string(n) + ")") + src.extension().string());
            }
        // Never overwrite an existing item, including dangling symbolic links.
        if (fs::exists(fs::symlink_status(dest, ec)))
            { error = "An item named '" + dest.filename().string() + "' already exists."; return false; }
        if (move)
            {
            if (RenamePath(source, dest.string(), error)) continue;
            if (errno != EXDEV) return false;
            }
        ec.clear();
        fs::copy(src, dest, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        if (!Result(ec, error)) return false;
        if (move) { fs::remove_all(src, ec); if (!Result(ec, error)) return false; }
        }
    error.clear();
    return true;
}
}
std::string WinErrorMessage(unsigned long code) { return std::strerror((int)code); }
bool PathExists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec));
}
bool IsDirectory(const std::string& path)
{
    std::error_code ec;
    return fs::is_directory(path, ec);
}
bool ListDirectory(const std::string& path, std::vector<FileEntry>& out, std::string& error)
{
    out.clear();
    std::error_code ec;
    fs::directory_iterator it(path, ec), end;
    if (!Result(ec, error)) return false;
    for (; it != end; it.increment(ec))
        {
        if (!Result(ec, error)) return false;
        FileEntry e;
        e.path = it->path().string();
        e.name = it->path().filename().string();
        e.nameWide = Utf8ToWide(e.name);
        e.isHidden = !e.name.empty() && e.name[0] == '.';
        struct stat st{};
        if (lstat(e.path.c_str(), &st) != 0) continue;
        e.isReparsePoint = S_ISLNK(st.st_mode);
        if (e.isReparsePoint) { struct stat target{}; if (stat(e.path.c_str(), &target) == 0) st = target; }
        e.isDirectory = S_ISDIR(st.st_mode);
        e.isReadOnly = access(e.path.c_str(), W_OK) != 0;
        e.size = e.isDirectory ? 0 : st.st_size;
        e.modified = st.st_mtime;
        if (!e.isDirectory)
            {
            e.extension = ToLowerAscii(it->path().extension().string());
            if (!e.extension.empty()) e.extension.erase(0, 1);
            }
        out.push_back(std::move(e));
        }
    return Result(ec, error);
}
std::vector<DriveInfo> GetDrives()
{
    std::vector<DriveInfo> out;
    auto add = [&](const std::string& path, const std::string& name)
        {
        for (const auto& d : out) if (d.root == path) return;
        DriveInfo d;
        d.root = path; d.letter = path; d.label = name; d.displayName = name; d.typeName = "Filesystem";
        std::error_code ec;
        const auto space = fs::space(path, ec);
        d.ready = !ec;
        if (!ec) { d.totalBytes = space.capacity; d.freeBytes = space.available; }
        out.push_back(std::move(d));
        };
    add("/", "Filesystem (/)");
    GVolumeMonitor* monitor = g_volume_monitor_get();
    GList* mounts = g_volume_monitor_get_mounts(monitor);
    for (GList* p = mounts; p; p = p->next)
        {
        GFile* root = g_mount_get_root(G_MOUNT(p->data));
        char* path = g_file_get_path(root);
        char* name = g_mount_get_name(G_MOUNT(p->data));
        if (path) add(path, name);
        g_free(path); g_free(name); g_object_unref(root);
        }
    g_list_free_full(mounts, g_object_unref);
    g_object_unref(monitor);
    return out;
}
std::vector<KnownFolder> GetKnownFolders()
{
    std::vector<KnownFolder> out{{KnownFolderKind::Home, "Home", g_get_home_dir()}};
    const KnownFolderKind kinds[] = {KnownFolderKind::Desktop, KnownFolderKind::Documents, KnownFolderKind::Downloads,
        KnownFolderKind::Pictures, KnownFolderKind::Music, KnownFolderKind::Videos};
    const GUserDirectory dirs[] = {G_USER_DIRECTORY_DESKTOP, G_USER_DIRECTORY_DOCUMENTS, G_USER_DIRECTORY_DOWNLOAD,
        G_USER_DIRECTORY_PICTURES, G_USER_DIRECTORY_MUSIC, G_USER_DIRECTORY_VIDEOS};
    const char* names[] = {"Desktop", "Documents", "Downloads", "Pictures", "Music", "Videos"};
    for (size_t i = 0; i < 6; ++i)
        if (const char* path = g_get_user_special_dir(dirs[i]); path && IsDirectory(path))
            out.push_back({kinds[i], names[i], path});
    return out;
}
std::string FileTypeName(const std::string& ext, bool dir)
{
    if (dir) return "File folder";
    if (ext.empty()) return "File";
    const std::string name = "file." + ext;
    char* type = g_content_type_guess(name.c_str(), nullptr, 0, nullptr);
    char* description = g_content_type_get_description(type);
    std::string result = description ? description : ext + " File";
    g_free(description); g_free(type);
    return result;
}
std::string FormatDateTime(int64_t ticks)
{
    const time_t value = ticks;
    struct tm local{};
    if (!localtime_r(&value, &local)) return "";
    char buf[128];
    std::strftime(buf, sizeof(buf), "%x %H:%M", &local);
    return buf;
}
bool CreateFolder(const std::string& path, std::string& error)
{
    std::error_code ec;
    if (!fs::create_directory(path, ec) && !ec) ec = std::make_error_code(std::errc::file_exists);
    return Result(ec, error);
}
bool CreateEmptyFile(const std::string& path, std::string& error)
{
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0) { error = std::strerror(errno); return false; }
    close(fd); error.clear(); return true;
}
bool RenamePath(const std::string& from, const std::string& to, std::string& error)
{
    if (syscall(SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), RENAME_NOREPLACE) == 0)
        { error.clear(); return true; }
    const int saved = errno;
    error = std::strerror(saved); errno = saved; return false;
}
bool DeletePermanently(const std::vector<std::string>& paths, std::string& error)
{
    for (const auto& path : paths)
        { std::error_code ec; fs::remove_all(path, ec); if (!Result(ec, error)) return false; }
    error.clear(); return true;
}
bool MoveToRecycleBin(const std::vector<std::string>& paths, std::string& error)
{
    for (const auto& path : paths)
        {
        GFile* file = g_file_new_for_path(path.c_str());
        GError* e = nullptr;
        bool ok = g_file_trash(file, nullptr, &e);
        g_object_unref(file);
        if (!GResult(ok, e, error)) return false;
        }
    return true;
}
bool CopyPaths(const std::vector<std::string>& sources, const std::string& dest, bool unique, std::string& error)
{ return Transfer(sources, dest, false, unique, error); }
bool MovePaths(const std::vector<std::string>& sources, const std::string& dest, std::string& error)
{ return Transfer(sources, dest, true, false, error); }
}
