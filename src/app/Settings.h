// User preferences, persisted as key=value lines in settings.ini beside the
// executable.
#pragma once

#include <string>
#include <vector>

namespace app
{

// Slots on the shortcut bar across the top of the window.
inline constexpr int kShortcutSlots = 10;

// How a folder's contents are laid out.
enum class ViewMode
{
    Details = 0,      // the table: name, date, type, size
    Thumbnails = 1,   // a grid of pictures (images) and big icons (everything else)
};

// Thumbnail edge lengths the View menu offers, in unscaled pixels.
inline constexpr int kThumbnailSizes[] = { 64, 96, 128, 192 };

struct Settings
{
    // One folder per slot; empty string means the slot is unassigned.
    std::vector<std::string> shortcuts = std::vector<std::string>(kShortcutSlots);

    int   themeIndex = 0;
    float uiScale = 1.0f;          // user zoom, independent of monitor DPI
    bool  showHidden = false;
    bool  showExtensions = true;
    bool  showNavPane = true;
    bool  showShortcutBar = true;
    bool  showStatusBar = true;
    bool  showDetailsPane = false;
    bool  openImagesInApp = true;  // double-click on an image: the built-in viewer instead of the shell
    ViewMode viewMode = ViewMode::Details;   // what a new view starts as
    int   thumbnailSize = 96;      // unscaled pixels, one of kThumbnailSizes
    std::string startPath;         // where the first tab opens; "" is This PC
    bool  rememberLastPath = true;

    int  windowWidth = 1280;
    int  windowHeight = 800;
    bool windowMaximized = false;
};

bool LoadSettings(const std::string& file, Settings& out);
bool SaveSettings(const std::string& file, const Settings& settings, std::string& error);

// The same parser the file loader uses, exposed so it can be tested.
void ApplySettingLine(Settings& s, const std::string& key, const std::string& value);
std::string SerializeSettings(const Settings& s);

} // namespace app
