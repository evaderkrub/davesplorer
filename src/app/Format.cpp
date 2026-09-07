#include "app/Format.h"

#include <cstdio>

namespace app
{

std::string WithThousands(uint64_t value)
{
    std::string digits = std::to_string(value);
    std::string out;
    const size_t n = digits.size();
    for (size_t i = 0; i < n; ++i)
        {
        if (i > 0 && (n - i) % 3 == 0) out += ',';
        out += digits[i];
        }
    return out;
}

std::string SizeInKB(uint64_t bytes)
{
    const uint64_t kb = (bytes + 1023) / 1024;
    return WithThousands(kb) + " KB";
}

std::string HumanSize(uint64_t bytes)
{
    if (bytes < 1024) return std::to_string(bytes) + (bytes == 1 ? " byte" : " bytes");
    const char* units[] = { "KB", "MB", "GB", "TB", "PB" };
    double value = (double)bytes;
    int unit = -1;
    while (value >= 1024.0 && unit < 4)
        {
        value /= 1024.0;
        ++unit;
        }
    // Explorer trims precision as the number grows: 1.20 KB, 12.3 MB, 123 GB.
    char buf[64];
    if (value < 10.0)       std::snprintf(buf, sizeof(buf), "%.2f %s", value, units[unit]);
    else if (value < 100.0) std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
    else                    std::snprintf(buf, sizeof(buf), "%.0f %s", value, units[unit]);
    return buf;
}

} // namespace app
