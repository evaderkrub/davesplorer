#include "ui/Textures.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace ui::textures
{

namespace
{

std::vector<ImTextureData*> g_live;      // registered and drawable
std::vector<ImTextureData*> g_retired;   // released, waiting for the renderer to destroy

} // namespace

ImTextureData* Create(int width, int height, const unsigned char* rgba)
{
    ImTextureData* tex = IM_NEW(ImTextureData)();
    tex->Create(ImTextureFormat_RGBA32, width, height);
    std::memcpy(tex->GetPixels(), rgba, (size_t)tex->GetSizeInBytes());
    tex->UseColors = true;
    // The whole surface counts as used, so a backend that uploads by rect
    // (rather than the whole texture on create) still sees all of it.
    tex->UsedRect = { 0, 0, (unsigned short)width, (unsigned short)height };
    ImGui::RegisterUserTexture(tex);
    g_live.push_back(tex);
    return tex;
}

void Release(ImTextureData* tex)
{
    if (!tex) return;
    g_live.erase(std::remove(g_live.begin(), g_live.end(), tex), g_live.end());
    // ImGui turns this into WantDestroy at the next NewFrame; the backend
    // then destroys it during render and reports Destroyed.
    tex->WantDestroyNextFrame = true;
    g_retired.push_back(tex);
}

void Pump()
{
    for (size_t i = 0; i < g_retired.size();)
        {
        ImTextureData* tex = g_retired[i];
        if (tex->Status == ImTextureStatus_Destroyed)
            {
            ImGui::UnregisterUserTexture(tex);
            IM_DELETE(tex);
            g_retired.erase(g_retired.begin() + (ptrdiff_t)i);
            }
        else
            {
            ++i;
            }
        }
}

void ReleaseAll()
{
    for (ImTextureData* tex : g_live) g_retired.push_back(tex);
    g_live.clear();
    for (ImTextureData* tex : g_retired)
        {
        ImGui::UnregisterUserTexture(tex);
        IM_DELETE(tex);
        }
    g_retired.clear();
}

} // namespace ui::textures
