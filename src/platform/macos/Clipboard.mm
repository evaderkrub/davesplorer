#include "platform/Clipboard.h"
#import <AppKit/AppKit.h>
#include <SDL3/SDL.h>
#include <cstring>

namespace platform
{
namespace
{
// The pasteboard owns the file list rather than SDL. SDL's macOS clipboard
// registers a custom type as a synthesized "dyn." identifier, which no other
// application recognizes, and reads back only the first pasteboard item,
// which is one file out of however many were copied. Writing the items
// Finder itself writes is what makes copy and paste cross the app boundary.
constexpr const char* kCut   = "com.davesplorer.cut";
constexpr const char* kPaths = "com.davesplorer.file-list";
NSPasteboard* Board()
{
    // Without the Cocoa driver there is no desktop session to share a
    // clipboard with: the tests run on the dummy driver, and a private
    // pasteboard keeps a test run from disturbing what the user has copied.
    static NSPasteboard* board = []
        {
        const char* driver = SDL_GetCurrentVideoDriver();
        return driver && std::strcmp(driver, "cocoa") == 0 ? NSPasteboard.generalPasteboard
                                                           : [NSPasteboard pasteboardWithUniqueName];
        }();
    return board;
}
NSDictionary* FileUrlsOnly() { return @{NSPasteboardURLReadingFileURLsOnlyKey: @YES}; }
NSString* CutType() { return [NSString stringWithUTF8String:kCut]; }
NSString* PathsType() { return [NSString stringWithUTF8String:kPaths]; }
}
bool SetClipboardFiles(const std::vector<std::string>& paths, bool cut, std::string& error)
{
    @autoreleasepool
        {
        NSPasteboard* board = Board();
        if (!board) { error = "The clipboard is not available."; return false; }
        NSMutableArray<NSPasteboardItem*>* items = [NSMutableArray array];
        NSMutableArray<NSString*>* names = [NSMutableArray array];
        for (const auto& path : paths)
            {
            NSString* text = [NSString stringWithUTF8String:path.c_str()];
            if (!text) { error = "Invalid clipboard file path."; return false; }
            NSPasteboardItem* item = [NSPasteboardItem new];
            [item setString:[NSURL fileURLWithPath:text].absoluteString forType:NSPasteboardTypeFileURL];
            [names addObject:text];
            [items addObject:item];
            }
        if (items.count == 0) { error = "Nothing to copy."; return false; }
        // A file URL comes back decomposed, because that is the form the
        // filesystem canonicalizes to; the app's own paste wants the bytes
        // it was given, so the list rides along verbatim on the first item.
        // Nothing on macOS cuts files through the pasteboard either, so the
        // cut flag is private in the same way: others see a plain copy.
        [items.firstObject setPropertyList:names forType:PathsType()];
        [items.firstObject setString:(cut ? @"1" : @"0") forType:CutType()];
        [board clearContents];
        if (![board writeObjects:items]) { error = "The clipboard rejected the file list."; return false; }
        error.clear();
        return true;
        }
}
bool GetClipboardFiles(std::vector<std::string>& paths, bool& cut)
{
    @autoreleasepool
        {
        paths.clear();
        cut = false;
        NSPasteboard* board = Board();
        if (!board) return false;
        id own = [board propertyListForType:PathsType()];
        if ([own isKindOfClass:NSArray.class])
            {
            for (id name in (NSArray*)own)
                if ([name isKindOfClass:NSString.class]) paths.emplace_back(((NSString*)name).UTF8String);
            }
        else
            {
            for (NSURL* url in [board readObjectsForClasses:@[NSURL.class] options:FileUrlsOnly()])
                if (url.path) paths.emplace_back(url.path.UTF8String);
            }
        cut = [[board stringForType:CutType()] isEqualToString:@"1"];
        return !paths.empty();
        }
}
bool ClipboardHasFiles()
{
    @autoreleasepool
        {
        NSPasteboard* board = Board();
        return board && [board canReadObjectForClasses:@[NSURL.class] options:FileUrlsOnly()];
        }
}
unsigned long ClipboardSequence()
{
    @autoreleasepool
        {
        NSPasteboard* board = Board();
        // The pasteboard's own count already moves for every writer, this
        // app included; nothing has to be tracked alongside it.
        return board ? (unsigned long)board.changeCount : 0;
        }
}
bool ClearClipboard()
{
    @autoreleasepool
        {
        NSPasteboard* board = Board();
        if (!board) return false;
        [board clearContents];
        return true;
        }
}
}
