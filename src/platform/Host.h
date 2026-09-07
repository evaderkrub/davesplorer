// The SDL3 window, renderer and frame loop that host the interface.
#pragma once

namespace platform
{

// Runs until the window closes. Returns the process exit code.
int RunApplication(int argc, char** argv);

} // namespace platform
