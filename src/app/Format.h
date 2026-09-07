// Number formatting the way Explorer does it.
#pragma once

#include <cstdint>
#include <string>

namespace app
{

// Details-view size column: whole kilobytes, rounded up, with thousands
// separators. 100 bytes is "1 KB", the way Explorer shows it.
std::string SizeInKB(uint64_t bytes);

// Status-bar style: "512 bytes", "1.20 KB", "3.5 MB", "12 GB".
std::string HumanSize(uint64_t bytes);

// "1,234,567"
std::string WithThousands(uint64_t value);

} // namespace app
