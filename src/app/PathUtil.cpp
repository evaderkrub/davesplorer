#ifndef _WIN32
#include "app/PathUtil.h"
#include <filesystem>
namespace app
{
std::string NormalizePath(const std::string& path)
{
    if (path.empty()) return "";
    // Linux names may contain backslashes and leading/trailing spaces.
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (ec) return path;
    std::string result = absolute.lexically_normal().string();
    while (result.size() > 1 && result.back() == '/') result.pop_back();
    return result;
}
bool IsDriveRoot(const std::string& path) { return path == "/"; }
std::string ParentPath(const std::string& path)
{
    if (path.empty() || path == "/") return "";
    return std::filesystem::path(path).parent_path().string();
}
std::string PathName(const std::string& path)
{
    if (path.empty()) return "This PC";
    if (path == "/") return "/";
    return std::filesystem::path(path).filename().string();
}
std::string JoinPath(const std::string& dir, const std::string& name)
{
    return (std::filesystem::path(dir) / name).string();
}
std::vector<Crumb> Breadcrumbs(const std::string& path)
{
    std::vector<Crumb> out{{"This PC", ""}};
    if (path.empty()) return out;
    std::string sofar;
    for (const auto& part : std::filesystem::path(path))
        {
        if (part.empty()) continue;
        sofar = JoinPath(sofar, part.string());
        out.push_back({part.string(), sofar});
        }
    return out;
}
bool IsValidFileName(const std::string& name, std::string& reason)
{
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos || name.find('\0') != std::string::npos)
        { reason = "Enter a name other than . or .., without a slash or NUL."; return false; }
    return true;
}
}
#else
#include "app/PathUtil.h"

#include <cctype>

namespace app
{

namespace
{

bool IsSep(char c) { return c == '\\' || c == '/'; }

bool LooksLikeDrive(const std::string& p)
{
    return p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':';
}

} // namespace

std::string NormalizePath(const std::string& path)
{
    std::string out;
    out.reserve(path.size() + 1);
    for (char c : path)
        out += (c == '/') ? '\\' : c;
    // Trim whitespace a user may have pasted around the path.
    while (!out.empty() && std::isspace((unsigned char)out.back())) out.pop_back();
    size_t start = 0;
    while (start < out.size() && std::isspace((unsigned char)out[start])) ++start;
    out.erase(0, start);
    if (out.empty()) return out;

    if (LooksLikeDrive(out))
        {
        out[0] = (char)std::toupper((unsigned char)out[0]);
        if (out.size() == 2) out += '\\';
        }
    // Collapse doubled separators, but keep a leading "\\" for UNC paths.
    std::string collapsed;
    for (size_t i = 0; i < out.size(); ++i)
        {
        if (out[i] == '\\' && i > 0 && out[i - 1] == '\\' && i != 1) continue;
        collapsed += out[i];
        }
    out = collapsed;
    while (out.size() > 1 && out.back() == '\\' && !(out.size() == 3 && LooksLikeDrive(out)))
        out.pop_back();
    return out;
}

bool IsDriveRoot(const std::string& path)
{
    return path.size() == 3 && LooksLikeDrive(path) && IsSep(path[2]);
}

std::string ParentPath(const std::string& path)
{
    if (path.empty() || IsDriveRoot(path)) return "";
    const size_t sep = path.find_last_of("\\/");
    if (sep == std::string::npos) return "";
    std::string parent = path.substr(0, sep);
    if (LooksLikeDrive(parent) && parent.size() == 2) parent += '\\';
    // A UNC share ("\\server\share") has no parent we can list.
    if (parent == "\\" || parent.empty()) return "";
    return parent;
}

std::string PathName(const std::string& path)
{
    if (path.empty()) return "This PC";
    if (IsDriveRoot(path)) return path;
    const size_t sep = path.find_last_of("\\/");
    if (sep == std::string::npos) return path;
    return path.substr(sep + 1);
}

std::string JoinPath(const std::string& dir, const std::string& name)
{
    if (dir.empty()) return name;
    if (IsSep(dir.back())) return dir + name;
    return dir + '\\' + name;
}

std::vector<Crumb> Breadcrumbs(const std::string& path)
{
    std::vector<Crumb> crumbs;
    crumbs.push_back({ "This PC", "" });
    if (path.empty()) return crumbs;

    size_t pos = 0;
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
        {
        // UNC: the server and share form one crumb, since neither is a
        // location on its own.
        const size_t s1 = path.find('\\', 2);
        const size_t s2 = (s1 == std::string::npos) ? std::string::npos : path.find('\\', s1 + 1);
        const size_t end = (s2 == std::string::npos) ? path.size() : s2;
        crumbs.push_back({ path.substr(0, end), path.substr(0, end) });
        pos = end;
        }
    else if (LooksLikeDrive(path))
        {
        crumbs.push_back({ path.substr(0, 2), path.substr(0, 2) + "\\" });
        pos = 2;
        }

    std::string sofar = crumbs.back().path;
    while (pos < path.size())
        {
        if (IsSep(path[pos]))
            {
            ++pos;
            continue;
            }
        size_t next = pos;
        while (next < path.size() && !IsSep(path[next])) ++next;
        const std::string part = path.substr(pos, next - pos);
        sofar = JoinPath(sofar, part);
        crumbs.push_back({ part, sofar });
        pos = next;
        }
    return crumbs;
}

bool IsValidFileName(const std::string& name, std::string& reason)
{
    if (name.empty())
        {
        reason = "Enter a name.";
        return false;
        }
    for (char c : name)
        {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|' || (unsigned char)c < 32)
            {
            reason = "A name can't contain any of the following characters: \\ / : * ? \" < > |";
            return false;
            }
        }
    if (name.back() == '.' || name.back() == ' ')
        {
        reason = "A name can't end with a space or a period.";
        return false;
        }
    if (name == "." || name == "..")
        {
        reason = "That name is reserved.";
        return false;
        }
    static const char* reserved[] = { "CON", "PRN", "AUX", "NUL",
                                      "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                      "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    std::string stem = name.substr(0, name.find('.'));
    for (char& c : stem) c = (char)std::toupper((unsigned char)c);
    for (const char* r : reserved)
        if (stem == r)
            {
            reason = "The specified device name is invalid.";
            return false;
            }
    return true;
}

} // namespace app

#endif
