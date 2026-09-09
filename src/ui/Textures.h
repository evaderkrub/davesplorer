// Textures the interface creates itself (an image being viewed), handed to
// whichever renderer backend is running through ImGui's texture list, the
// same route the font atlas takes. Nothing here knows about SDL.
//
// The renderer creates a texture the first time it renders a frame that
// lists it, and destroys it a frame or two after it is released; the
// ImTextureData object is kept until the renderer confirms that.
#pragma once

#include "imgui.h"

namespace ui::textures
{

// An RGBA8 texture, width * height * 4 bytes, registered with ImGui. Draw it
// with tex->GetTexRef().
ImTextureData* Create(int width, int height, const unsigned char* rgba);

// Stops drawing it; the memory goes once the renderer has let go.
void Release(ImTextureData* tex);

// Once per frame, inside the frame: frees textures the renderer has now
// destroyed.
void Pump();

// At shutdown, after the renderer backend has shut down and before the
// ImGui context is destroyed.
void ReleaseAll();

} // namespace ui::textures
