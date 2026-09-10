#include "platform/FileSystem.h"
#include "platform/Strings.h"
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <filesystem>
#include <ctime>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>

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
bool NSResult(bool ok, NSError* failure, std::string& error)
{
    error = ok ? "" : (failure ? failure.localizedDescription.UTF8String : "The operation failed.");
    return ok;
}
NSURL* FileURL(const std::string& path)
{
    NSString* text = [NSString stringWithUTF8String:path.c_str()];
    return text ? [NSURL fileURLWithPath:text] : nil;
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
        struct stat st{};
        if (lstat(e.path.c_str(), &st) != 0) continue;
        // A dot name hides an item on every Unix; macOS also has a per-file
        // flag, which is how /Volumes and the Finder's own metadata vanish.
        e.isHidden = (!e.name.empty() && e.name[0] == '.') || (st.st_flags & UF_HIDDEN) != 0;
        e.isReparsePoint = S_ISLNK(st.st_mode);
        if (e.isReparsePoint) { struct stat target{}; if (stat(e.path.c_str(), &target) == 0) st = target; }
        e.isDirectory = S_ISDIR(st.st_mode);
        e.isReadOnly = access(e.path.c_str(), W_OK) != 0;
        e.size = e.isDirectory ? 0 : (uint64_t)st.st_size;
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
    @autoreleasepool
        {
        std::vector<DriveInfo> out;
        auto add = [&](const std::string& path, const std::string& name)
            {
            for (const auto& d : out) if (d.root == path) return;
            DriveInfo d;
            d.root = path; d.letter = path; d.label = name; d.displayName = name; d.typeName = "Volume";
            std::error_code ec;
            const auto space = fs::space(path, ec);
            d.ready = !ec;
            if (!ec) { d.totalBytes = space.capacity; d.freeBytes = space.available; }
            out.push_back(std::move(d));
            };
        auto volumeName = [](NSURL* url, const std::string& fallback)
            {
            NSString* name = nil;
            [url getResourceValue:&name forKey:NSURLVolumeLocalizedNameKey error:nil];
            return name ? std::string(name.UTF8String) : fallback;
            };
        add("/", volumeName([NSURL fileURLWithPath:@"/"], "Filesystem (/)"));
        NSArray<NSURL*>* mounts =
            [NSFileManager.defaultManager mountedVolumeURLsIncludingResourceValuesForKeys:
                @[NSURLVolumeLocalizedNameKey] options:NSVolumeEnumerationSkipHiddenVolumes];
        for (NSURL* url in mounts)
            {
            NSString* path = url.path;
            if (path) add(path.UTF8String, volumeName(url, path.lastPathComponent.UTF8String));
            }
        return out;
        }
}
std::vector<KnownFolder> GetKnownFolders()
{
    @autoreleasepool
        {
        std::vector<KnownFolder> out{{KnownFolderKind::Home, "Home", NSHomeDirectory().UTF8String}};
        const KnownFolderKind kinds[] = {KnownFolderKind::Desktop, KnownFolderKind::Documents,
            KnownFolderKind::Downloads, KnownFolderKind::Pictures, KnownFolderKind::Music,
            KnownFolderKind::Videos};
        const NSSearchPathDirectory dirs[] = {NSDesktopDirectory, NSDocumentDirectory,
            NSDownloadsDirectory, NSPicturesDirectory, NSMusicDirectory, NSMoviesDirectory};
        const char* names[] = {"Desktop", "Documents", "Downloads", "Pictures", "Music", "Movies"};
        for (size_t i = 0; i < 6; ++i)
            {
            NSArray<NSString*>* found = NSSearchPathForDirectoriesInDomains(dirs[i], NSUserDomainMask, YES);
            if (found.count == 0) continue;
            const std::string path = found.firstObject.UTF8String;
            if (IsDirectory(path)) out.push_back({kinds[i], names[i], path});
            }
        return out;
        }
}
std::string FileTypeName(const std::string& ext, bool dir)
{
    if (dir) return "File folder";
    if (ext.empty()) return "File";
    @autoreleasepool
        {
        NSString* extension = [NSString stringWithUTF8String:ext.c_str()];
        if (extension)
            {
            UTType* type = [UTType typeWithFilenameExtension:extension];
            if (type.localizedDescription) return type.localizedDescription.UTF8String;
            }
        return ToLowerAscii(ext) + " File";
        }
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
    // RENAME_EXCL is the atomic "fail if the destination exists" that a
    // plain rename() lacks; Transfer relies on errno surviving the call.
    if (renamex_np(from.c_str(), to.c_str(), RENAME_EXCL) == 0) { error.clear(); return true; }
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
    @autoreleasepool
        {
        for (const auto& path : paths)
            {
            NSURL* url = FileURL(path);
            if (!url) { error = "Invalid path."; return false; }
            NSError* failure = nil;
            const bool ok = [NSFileManager.defaultManager trashItemAtURL:url
                                                        resultingItemURL:nil error:&failure];
            if (!NSResult(ok, failure, error)) return false;
            }
        return true;
        }
}
bool CopyPaths(const std::vector<std::string>& sources, const std::string& dest, bool unique, std::string& error)
{ return Transfer(sources, dest, false, unique, error); }
bool MovePaths(const std::vector<std::string>& sources, const std::string& dest, std::string& error)
{ return Transfer(sources, dest, true, false, error); }
}
