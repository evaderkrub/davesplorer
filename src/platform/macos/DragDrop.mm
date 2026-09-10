#include "platform/DragDrop.h"
namespace platform
{
bool DragFilesOut(const std::vector<std::string>&, DragOutcome& outcome, std::string& error)
{
    outcome = DragOutcome::Cancelled;
    error = "Dragging to other applications is not available on macOS yet. Use Copy and Paste instead.";
    return false;
}
}
