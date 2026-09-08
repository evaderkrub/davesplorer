// Where the executable lives. Assets resolve from here on every platform.
// Windows also keeps settings/layout here; Linux uses its XDG config directory.
#pragma once

#include <string>

namespace platform
{

// UTF-8, no trailing separator.
std::string ExecutableDir();

} // namespace platform
