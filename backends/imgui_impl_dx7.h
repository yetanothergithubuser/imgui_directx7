// dear imgui: Renderer Backend for DirectX7
// This needs to be used along with a Platform Backend (e.g. Win32)

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct IDirect3DDevice7;
struct IDirectDraw7;

IMGUI_IMPL_API bool     ImGui_ImplDX7_Init(IDirectDraw7* ddraw, IDirect3DDevice7* device);
IMGUI_IMPL_API void     ImGui_ImplDX7_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplDX7_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplDX7_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplDX7_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplDX7_InvalidateDeviceObjects();

#endif // #ifndef IMGUI_DISABLE
