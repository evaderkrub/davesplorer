#include "platform/Shell.h"
#import <AppKit/AppKit.h>
#include <filesystem>

namespace platform
{
namespace
{
NSURL* FileURL(const std::string& path)
{
    NSString* text = [NSString stringWithUTF8String:path.c_str()];
    return text ? [NSURL fileURLWithPath:text] : nil;
}
}
bool OpenWithShell(const std::string& path, std::string& error)
{
    @autoreleasepool
        {
        NSURL* url = FileURL(path);
        if (url && [[NSWorkspace sharedWorkspace] openURL:url]) { error.clear(); return true; }
        error = "Could not open '" + path + "'.";
        return false;
        }
}
bool OpenFolderInExplorer(const std::string& folder, std::string& error)
{ return OpenWithShell(folder.empty() ? "/" : folder, error); }
bool ShowInExplorer(const std::string& path, std::string& error)
{
    @autoreleasepool
        {
        NSString* text = [NSString stringWithUTF8String:path.c_str()];
        // Rooted at the empty path so Finder reuses a window already showing
        // the parent, which is what selecting an item there normally does.
        if (text && [[NSWorkspace sharedWorkspace] selectFile:text inFileViewerRootedAtPath:@""])
            { error.clear(); return true; }
        }
    return OpenWithShell(std::filesystem::path(path).parent_path().string(), error);
}
bool OpenTerminalAt(const std::string& dir, std::string& error)
{
    @autoreleasepool
        {
        NSString* text = [NSString stringWithUTF8String:dir.c_str()];
        if (!text) { error = "Invalid folder path."; return false; }
        // Arguments go to the launcher as argv: a folder name must never be
        // read back as shell code.
        NSTask* task = [[NSTask alloc] init];
        task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/open"];
        task.arguments = @[@"-a", @"Terminal", @"--", text];
        NSError* failure = nil;
        if ([task launchAndReturnError:&failure]) { error.clear(); return true; }
        error = failure ? failure.localizedDescription.UTF8String : "Could not open Terminal.";
        return false;
        }
}
}
