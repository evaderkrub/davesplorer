// Where the executable lives. Every runtime path (assets, settings, layout)
// is resolved against this, never the working directory, so a shortcut or a
// launch from another drive cannot break a portable build.
#pragma once

#include <string>

namespace platform
{

// UTF-8, no trailing separator.
std::string ExecutableDir();

} // namespace platform
