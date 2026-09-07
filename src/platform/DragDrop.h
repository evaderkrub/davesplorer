// Dragging files out to other programs through OLE.
//
// Dropping INTO this app is SDL's job: it registers the window's drop target
// and reports drops as events. Dragging OUT needs a drop source, which SDL
// does not provide, so it lives here.
#pragma once

#include <string>
#include <vector>

namespace platform
{

enum class DragOutcome
{
    Cancelled,
    Copied,
    Moved,
    Linked,
};

// Blocks until the user drops or cancels. Must be called on the thread that
// created the window, with OLE initialized (SDL does that at startup). The
// receiving program performs the file operation itself.
bool DragFilesOut(const std::vector<std::string>& paths, DragOutcome& outcome, std::string& error);

} // namespace platform
