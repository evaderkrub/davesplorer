#include <filesystem>
#include "ui/MainWindow.h"
#include "ui/DragDrop.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/Navigation.h"
#include "app/PathUtil.h"
#include "platform/FileSystem.h"
#include "platform/Strings.h"

#include "imgui.h"

#include <algorithm>

namespace ui
{

namespace
{

const std::vector<std::string>& ChildFolders(UiState& ui, const app::Settings& settings, const std::string& path)
{
    auto it = ui.treeChildren.find(path);
    if (it != ui.treeChildren.end()) return it->second;
    std::vector<std::string> names;
    std::vector<platform::FileEntry> entries;
    std::string error;
    if (platform::ListDirectory(path, entries, error))
        {
        std::vector<const platform::FileEntry*> dirs;
        for (const platform::FileEntry& e : entries)
            if (e.isDirectory && (settings.showHidden || !(e.isHidden || e.isSystem))) dirs.push_back(&e);
        std::sort(dirs.begin(), dirs.end(), [](const platform::FileEntry* a, const platform::FileEntry* b) {
            return platform::NaturalCompare(a->nameWide, b->nameWide) < 0;
        });
        for (const platform::FileEntry* d : dirs) names.push_back(d->name);
        }
    return ui.treeChildren.emplace(path, std::move(names)).first->second;
}

void Navigate(app::AppState& state, UiState& ui, const std::string& path)
{
    std::string error;
    if (!app::NavigateTo(state.active(), path, error)) ShowError(ui, error);
}

// A folder node: click on the label navigates, the arrow expands. Children
// are listed lazily the first time the node opens, and only one level deep,
// so a drive with a million folders costs one directory read.
void DrawFolderNode(app::AppState& state, UiState& ui, const std::string& path, const std::string& label,
                    const char* icon, int depth)
{
    const std::string current = state.active().path;
    const bool isCurrent = current == path;
    const bool onPath = !isCurrent && current.size() > path.size() &&
                        current.compare(0, path.size(), path) == 0 &&
                        (path.back() == std::filesystem::path::preferred_separator || current[path.size()] == std::filesystem::path::preferred_separator);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_NavLeftJumpsToParent;
    if (isCurrent) flags |= ImGuiTreeNodeFlags_Selected;
    // Auto-open the branch that leads to where the user is, so the tree
    // follows navigation done elsewhere; only once, so it can be collapsed.
    if (onPath && depth < 12) ImGui::SetNextItemOpen(true, ImGuiCond_Once);

    const std::string shown = std::string(icon) + "  " + label + "###" + label;
    ImGui::PushStyleColor(ImGuiCol_Text, isCurrent ? CurrentPalette().text : CurrentPalette().textMuted);
    const bool open = ImGui::TreeNodeEx(shown.c_str(), flags);
    ImGui::PopStyleColor();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) Navigate(state, ui, path);
    FileDropTarget(state, ui, path);

    if (open)
        {
        for (const std::string& child : ChildFolders(ui, state.settings, path))
            DrawFolderNode(state, ui, app::JoinPath(path, child), child, ICON_MD_FOLDER, depth + 1);
        ImGui::TreePop();
        }
}

} // namespace

void DrawNavPane(app::AppState& state, UiState& ui)
{
    const app::Tab& tab = state.active();
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetStyle().IndentSpacing * 0.8f);

    if (ImGui::TreeNodeEx(ICON_MD_STAR "  Quick access###QuickAccess",
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
        {
        for (const platform::KnownFolder& kf : state.quickAccess)
            {
            const std::string shown = std::string(IconForKnownFolder(kf.kind)) + "  " + kf.name + "###" + kf.name;
            ImGui::PushStyleColor(ImGuiCol_Text, tab.path == kf.path ? CurrentPalette().text : CurrentPalette().textMuted);
            ImGui::TreeNodeEx(shown.c_str(),
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth |
                (tab.path == kf.path ? ImGuiTreeNodeFlags_Selected : 0));
            ImGui::PopStyleColor();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) Navigate(state, ui, kf.path);
            FileDropTarget(state, ui, kf.path);
            }
        ImGui::TreePop();
        }

    ImGui::Spacing();
    const bool thisPcCurrent = tab.path.empty();
    ImGui::PushStyleColor(ImGuiCol_Text, thisPcCurrent ? CurrentPalette().text : CurrentPalette().textMuted);
    const bool pcOpen = ImGui::TreeNodeEx(ICON_MD_COMPUTER "  This PC###ThisPC",
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
        (thisPcCurrent ? ImGuiTreeNodeFlags_Selected : 0));
    ImGui::PopStyleColor();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) Navigate(state, ui, "");
    if (pcOpen)
        {
        for (const platform::DriveInfo& d : state.drives)
            {
            if (!d.ready) continue;
            const char* icon = d.typeName == "Removable Disk" ? ICON_MD_USB
                             : d.typeName == "CD Drive"       ? ICON_MD_ALBUM
                             : d.typeName == "Network Drive"  ? ICON_MD_CLOUD
                                                              : ICON_MD_STORAGE;
            DrawFolderNode(state, ui, d.root, d.displayName, icon, 0);
            }
        ImGui::TreePop();
        }
    ImGui::PopStyleVar();
}

} // namespace ui
