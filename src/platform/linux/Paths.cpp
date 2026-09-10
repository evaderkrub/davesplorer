#include "platform/Paths.h"
#include <glib.h>
#include <filesystem>
#include <stdexcept>
namespace platform
{
std::string ExecutableDir()
{
    return std::filesystem::read_symlink("/proc/self/exe").parent_path().string();
}
std::string UserConfigDir()
{
    return g_get_user_config_dir();
}
}
