#include "ui/Widgets.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace ui
{

bool IconButton(const char* id, const char* icon, const char* tooltip, bool enabled)
{
    const std::string label = std::string(icon) + "###" + id;
    ImGui::BeginDisabled(!enabled);
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    const bool pressed = ImGui::Button(label.c_str());
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    if (tooltip && enabled) TipOnHover(tooltip);
    return pressed;
}

void TipOnHover(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", text);
}

void MutedText(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().textMuted);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void FaintText(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().textFaint);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

namespace
{

enum class Kind { Folder, Drive, File, Image, Video, Audio, Code, Archive, Executable, Document, Pdf, Text };

bool In(std::string_view ext, std::initializer_list<const char*> list)
{
    for (const char* e : list)
        if (ext == e) return true;
    return false;
}

Kind KindOf(const platform::FileEntry& e)
{
    if (e.isDirectory) return (e.path.size() == 3 && e.path[1] == ':') ? Kind::Drive : Kind::Folder;
    const std::string_view x = e.extension;
    if (In(x, { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "ico", "tif", "tiff", "heic" })) return Kind::Image;
    if (In(x, { "mp4", "mkv", "avi", "mov", "wmv", "webm", "m4v" })) return Kind::Video;
    if (In(x, { "mp3", "wav", "flac", "ogg", "m4a", "aac", "wma" })) return Kind::Audio;
    if (In(x, { "zip", "7z", "rar", "tar", "gz", "bz2", "xz", "cab", "iso" })) return Kind::Archive;
    if (In(x, { "exe", "msi", "bat", "cmd", "com", "ps1", "lnk" })) return Kind::Executable;
    if (In(x, { "pdf" })) return Kind::Pdf;
    if (In(x, { "doc", "docx", "xls", "xlsx", "ppt", "pptx", "odt", "rtf" })) return Kind::Document;
    if (In(x, { "txt", "md", "log", "ini", "cfg", "csv" })) return Kind::Text;
    if (In(x, { "c", "cpp", "h", "hpp", "cs", "py", "js", "ts", "json", "xml", "html", "css", "java", "rs", "go",
                "cmake", "sh", "yaml", "yml", "toml", "sql" }))
        return Kind::Code;
    return Kind::File;
}

} // namespace

const char* IconForEntry(const platform::FileEntry& entry)
{
    switch (KindOf(entry))
        {
        case Kind::Folder:     return ICON_MD_FOLDER;
        case Kind::Drive:      return ICON_MD_STORAGE;
        case Kind::Image:      return ICON_MD_IMAGE;
        case Kind::Video:      return ICON_MD_MOVIE;
        case Kind::Audio:      return ICON_MD_AUDIOTRACK;
        case Kind::Code:       return ICON_MD_CODE;
        case Kind::Archive:    return ICON_MD_FOLDER_ZIP;
        case Kind::Executable: return ICON_MD_SETTINGS_APPLICATIONS;
        case Kind::Document:   return ICON_MD_ARTICLE;
        case Kind::Pdf:        return ICON_MD_PICTURE_AS_PDF;
        case Kind::Text:       return ICON_MD_DESCRIPTION;
        case Kind::File:       break;
        }
    return ICON_MD_INSERT_DRIVE_FILE;
}

ImU32 IconTintForEntry(const platform::FileEntry& entry)
{
    const Palette& p = CurrentPalette();
    switch (KindOf(entry))
        {
        case Kind::Folder:     return p.iconFolder;
        case Kind::Drive:      return p.iconDrive;
        case Kind::Image:      return p.iconImage;
        case Kind::Video:
        case Kind::Audio:      return p.iconMedia;
        case Kind::Code:       return p.iconCode;
        case Kind::Archive:    return p.iconArchive;
        case Kind::Executable: return p.iconExecutable;
        case Kind::Pdf:        return p.danger;
        case Kind::Document:
        case Kind::Text:
        case Kind::File:       break;
        }
    return p.iconFile;
}

const char* IconForKnownFolder(platform::KnownFolderKind kind)
{
    switch (kind)
        {
        case platform::KnownFolderKind::Home:      return ICON_MD_HOME;
        case platform::KnownFolderKind::Desktop:   return ICON_MD_DESKTOP_WINDOWS;
        case platform::KnownFolderKind::Documents: return ICON_MD_DESCRIPTION;
        case platform::KnownFolderKind::Downloads: return ICON_MD_DOWNLOAD;
        case platform::KnownFolderKind::Pictures:  return ICON_MD_IMAGE;
        case platform::KnownFolderKind::Music:     return ICON_MD_MUSIC_NOTE;
        case platform::KnownFolderKind::Videos:    return ICON_MD_MOVIE;
        }
    return ICON_MD_FOLDER;
}

} // namespace ui
