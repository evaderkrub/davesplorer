// User preferences, persisted as key=value lines in settings.ini beside the
// executable.
#pragma once

#include <string>

namespace app
{

struct Settings
{
    int   themeIndex = 0;
    float uiScale = 1.0f;          // user zoom, independent of monitor DPI
    bool  showHidden = false;
    bool  showExtensions = true;
    bool  showNavPane = true;
    bool  showStatusBar = true;
    bool  showDetailsPane = false;
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
