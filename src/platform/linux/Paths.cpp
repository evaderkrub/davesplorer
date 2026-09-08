#include "platform/Paths.h"
#include <filesystem>
#include <stdexcept>
namespace platform
{
std::string ExecutableDir()
{
    return std::filesystem::read_symlink("/proc/self/exe").parent_path().string();
}
}
