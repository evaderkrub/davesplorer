#include "ui/MainWindow.h"
#include "ui/Fonts.h"
#include "ui/IconsMaterialDesign.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include "app/FileOps.h"
#include "app/PathUtil.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <functional>

namespace ui
{

namespace
{

constexpr const char* kAboutTitle     = "About Davesplorer";
constexpr const char* kNewFolderTitle = "New folder";
constexpr const char* kNewFileTitle   = "New text file";
constexpr const char* kRenameTitle    = "Rename";
constexpr const char* kDeleteTitle    = "Delete";
// Not "Davesplorer": that is the host window's name, and two windows cannot
// share one.
constexpr const char* kErrorTitle     = "Error";

const char* TitleFor(Dialog d)
{
    switch (d)
        {
        case Dialog::About:     return kAboutTitle;
        case Dialog::NewFolder: return kNewFolderTitle;
        case Dialog::NewFile:   return kNewFileTitle;
        case Dialog::Rename:    return kRenameTitle;
        case Dialog::Delete:    return kDeleteTitle;
        case Dialog::Error:     return kErrorTitle;
        case Dialog::None:      break;
        }
    return "";
}

void CenterNext()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

bool BeginModal(const char* title)
{
    CenterNext();
    return ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
}

// The key that opened a dialog (Enter in the address bar, Delete in the
// list) is still "pressed" on the frame the dialog first draws, so a
// dialog must not react to keys on that frame or it closes at once.
bool EscapePressed()
{
    return !ImGui::IsWindowAppearing() && ImGui::IsKeyPressed(ImGuiKey_Escape);
}

bool EnterPressed()
{
    return !ImGui::IsWindowAppearing() &&
           (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter));
}

// Name-entry dialogs share a shape: a text field, OK/Cancel, Enter submits,
// Escape cancels, and a failure keeps the dialog open with the reason.
void DrawNameDialog(const char* title, const char* prompt, UiState& ui, const std::function<bool(const std::string&, std::string&)>& apply)
{
    if (!BeginModal(title)) return;
    ImGui::TextUnformatted(prompt);
    ImGui::SetNextItemWidth(360.0f * ImGui::GetStyle().FontScaleMain * ImGui::GetStyle().FontScaleDpi);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputText("##name", &ui.dialogText, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (!ui.dialogMessage.empty())
        {
        ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().danger);
        ImGui::TextWrapped("%s", ui.dialogMessage.c_str());
        ImGui::PopStyleColor();
        }
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(100, 0))) submit = true;
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel", ImVec2(100, 0)) || EscapePressed();
    if (submit)
        {
        std::string error;
        if (apply(ui.dialogText, error))
            {
            ui.dialogMessage.clear();
            ImGui::CloseCurrentPopup();
            }
        else
            {
            ui.dialogMessage = error;
            }
        }
    else if (cancel)
        {
        ui.dialogMessage.clear();
        ImGui::CloseCurrentPopup();
        }
    ImGui::EndPopup();
}

} // namespace

void DrawDialogs(app::AppState& state, UiState& ui)
{
    if (ui.dialogRequested && ui.dialog != Dialog::None)
        {
        ui.dialogRequested = false;
        ImGui::OpenPopup(TitleFor(ui.dialog));
        }
    app::Tab& tab = state.active();

    // --- About: always modal, per the brief.
    if (BeginModal(kAboutTitle))
        {
        ImGui::PushFont(fonts::Bold(), fonts::Size() * 1.6f);
        ImGui::TextUnformatted(ICON_MD_FOLDER_OPEN "  Davesplorer");
        ImGui::PopFont();
        MutedText("A Windows File Explorer clone, chiseled by hand.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Version 0.1.0");
        MutedText(state.buildInfo.c_str());
        MutedText(("Dear ImGui " IMGUI_VERSION " (docking) with SDL3"));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0)) || EscapePressed() || EnterPressed()) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        }

    DrawNameDialog(kNewFolderTitle, "Folder name:", ui, [&](const std::string& name, std::string& error) {
        return app::CreateFolderIn(tab, name, error);
    });
    DrawNameDialog(kNewFileTitle, "File name:", ui, [&](const std::string& name, std::string& error) {
        return app::CreateFileIn(tab, name, error);
    });
    DrawNameDialog(kRenameTitle, "New name:", ui, [&](const std::string& name, std::string& error) {
        return app::RenameEntry(tab, ui.dialogEntry, name, error);
    });

    if (BeginModal(kDeleteTitle))
        {
        ImGui::TextUnformatted(ui.deletePermanent ? ICON_MD_DELETE_FOREVER : ICON_MD_DELETE);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", ui.dialogMessage.c_str());
        ImGui::Spacing();
        bool close = false;
        if (ImGui::Button("Delete", ImVec2(100, 0)) || EnterPressed())
            {
            std::string error;
            if (!app::DeleteSelected(tab, ui.deletePermanent, error)) ShowError(ui, error);
            close = true;
            }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || EscapePressed()) close = true;
        if (close) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        }

    if (BeginModal(kErrorTitle))
        {
        ImGui::PushStyleColor(ImGuiCol_Text, CurrentPalette().danger);
        ImGui::TextUnformatted(ICON_MD_ERROR_OUTLINE);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f * ImGui::GetStyle().FontScaleMain);
        ImGui::TextUnformatted(ui.dialogMessage.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 0)) || EscapePressed() || EnterPressed()) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        }

    // A ShowError raised while another modal was closing this frame is
    // opened on the next pass through here.
    if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && !ui.dialogRequested)
        ui.dialog = Dialog::None;
}

} // namespace ui
