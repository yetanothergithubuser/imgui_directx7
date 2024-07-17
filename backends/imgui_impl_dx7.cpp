// dear imgui: Renderer Backend for DirectX7
// This needs to be used along with a Platform Backend (e.g. Win32)

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// adapted for DirectX7 by @yetanothergithubuser
// 2024-07-17: Initial version (based on DirectX9 version)

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_dx7.h"

// DirectX
#define DIRECT3D_VERSION 0x0700
#include <d3d.h>

// DirectX data
struct ImGui_ImplDX7_Data
{
    LPDIRECT3DDEVICE7           pd3dDevice;
    LPDIRECTDRAW7               pDD;
    LPDIRECT3DVERTEXBUFFER7     pVB;
    LPWORD                      pIB;
    LPDIRECTDRAWSURFACE7        FontTexture;
    int                         VertexBufferSize;
    int                         IndexBufferSize;

    ImGui_ImplDX7_Data()        { memset((void*)this, 0, sizeof(*this)); VertexBufferSize = 5000; IndexBufferSize = 10000; }
};

struct CUSTOMVERTEX
{
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_DX9_ARGB(_COL)     (_COL)
#else
#define IMGUI_COL_TO_DX9_ARGB(_COL)     (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

// non-D3DX version, calculates a plane from a point and a normal
// see D3DXPlaneFromPointNormal in d3dxmath.h
D3DVALUE* PlaneFromPointNormal(D3DVALUE* plane, D3DVECTOR* position, D3DVECTOR* normal)
{
    float* result = plane;

    if (!plane || !position || !normal)
        return nullptr;

    plane[0] = normal->x;
    plane[1] = normal->y;
    plane[2] = normal->z;
    plane[3] = -(position->y * normal->y + position->z * normal->z + position->x * normal->x);

    return result;
}

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplDX7_Data* ImGui_ImplDX7_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX7_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

// Functions
static void ImGui_ImplDX7_SetupRenderState(ImDrawData* draw_data)
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();

    // Setup viewport
    D3DVIEWPORT7 vp;
    vp.dwX = vp.dwY = 0;
    vp.dwWidth = (DWORD)draw_data->DisplaySize.x;
    vp.dwHeight = (DWORD)draw_data->DisplaySize.y;
    vp.dvMinZ = 0.0f;
    vp.dvMaxZ = 1.0f;
    bd->pd3dDevice->SetViewport(&vp);

    // Setup render state
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_RANGEFOGENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_STENCILENABLE, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
    bd->pd3dDevice->SetRenderState(D3DRENDERSTATE_CLIPPLANEENABLE, D3DCLIPPLANE0 | D3DCLIPPLANE1 | D3DCLIPPLANE2 | D3DCLIPPLANE3);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    bd->pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    bd->pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
    bd->pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFN_LINEAR);

    // Setup orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    // Being agnostic of whether <d3dx9.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
    {
        float L = draw_data->DisplayPos.x + 0.5f;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
        float T = draw_data->DisplayPos.y + 0.5f;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
        D3DMATRIX mat_identity = { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f };
        D3DMATRIX mat_projection =
        {
            2.0f / (R - L),   0.0f,         0.0f,  0.0f,
            0.0f,         2.0f / (T - B),   0.0f,  0.0f,
            0.0f,         0.0f,         0.5f,  0.0f,
            (L + R) / (L - R),  (T + B) / (B - T),  0.5f,  1.0f
        };
        bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &mat_identity);
        bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &mat_identity);
        bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &mat_projection);
    }
}

// Render function.
void ImGui_ImplDX7_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Create and grow buffers if needed
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;

        LPDIRECT3D7 pd3d = nullptr;
        bd->pd3dDevice->GetDirect3D(&pd3d);

        D3DVERTEXBUFFERDESC vbdesc;
        ZeroMemory(&vbdesc, sizeof(D3DVERTEXBUFFERDESC));
        vbdesc.dwSize = sizeof(D3DVERTEXBUFFERDESC);
        vbdesc.dwCaps = 0L;
        vbdesc.dwFVF = D3DFVF_CUSTOMVERTEX;
        vbdesc.dwNumVertices = bd->VertexBufferSize;
        if (pd3d->CreateVertexBuffer(&vbdesc, &bd->pVB, 0) != D3D_OK)
        {
            return;
        }
    }
    if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (bd->pIB) { delete[] bd->pIB; bd->pIB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        bd->pIB = new WORD[bd->IndexBufferSize * sizeof(ImDrawIdx)];
    }

    // Backup the states
    DWORD d3d7_state_block = 0;
    if (bd->pd3dDevice->CreateStateBlock(D3DSBT_ALL, &d3d7_state_block) != D3D_OK)
        return;
    if (bd->pd3dDevice->CaptureStateBlock(d3d7_state_block) != D3D_OK)
    {
        bd->pd3dDevice->DeleteStateBlock(d3d7_state_block);
        return;
    }

    // Backup the transforms
    D3DMATRIX last_world, last_view, last_projection;
    bd->pd3dDevice->GetTransform(D3DTRANSFORMSTATE_WORLD, &last_world);
    bd->pd3dDevice->GetTransform(D3DTRANSFORMSTATE_VIEW, &last_view);
    bd->pd3dDevice->GetTransform(D3DTRANSFORMSTATE_PROJECTION, &last_projection);

    // Allocate buffers
    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    DWORD vtx_size = (draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX));
    if (bd->pVB->Lock(DDLOCK_DISCARDCONTENTS, (void**)&vtx_dst, &vtx_size) != D3D_OK)
    {
        bd->pd3dDevice->DeleteStateBlock(d3d7_state_block);
        return;
    }
    idx_dst = bd->pIB;

    // Copy and convert all vertices into a single contiguous buffer, convert colors to DX9 default format.
    // FIXME-OPT: This is a minor waste of resource, the ideal is to use imconfig.h and
    //  1) to avoid repacking colors:   #define IMGUI_USE_BGRA_PACKED_COLOR
    //  2) to avoid repacking vertices: #define IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT struct ImDrawVert { ImVec2 pos; float z; ImU32 col; ImVec2 uv; }
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
        for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
        {
            vtx_dst->pos[0] = vtx_src->pos.x;
            vtx_dst->pos[1] = vtx_src->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col = IMGUI_COL_TO_DX9_ARGB(vtx_src->col);
            vtx_dst->uv[0] = vtx_src->uv.x;
            vtx_dst->uv[1] = vtx_src->uv.y;
            vtx_dst++;
            vtx_src++;
        }
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    bd->pVB->Unlock();

    // Setup desired DX state
    ImGui_ImplDX7_SetupRenderState(draw_data);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplDX7_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply plane clipping
                D3DVALUE plane[4][4] = { 0 };
                D3DVECTOR position = { clip_min.x, 0.0f, 0.0f };
                D3DVECTOR normal = { 1.0f, 0.0f, 0.0f };
                PlaneFromPointNormal(plane[0], &position, &normal);
                position = { clip_max.x, 0.0f, 0.0f };
                normal = { -1.0f, 0.0f, 0.0f };
                PlaneFromPointNormal(plane[1], &position, &normal);
                position = { 0.0f, clip_min.y, 0.0f };
                normal = { 0.0f, 1.0f, 0.0f };
                PlaneFromPointNormal(plane[2], &position, &normal);
                position = { 0.0f, clip_max.y, 0.0f };
                normal = { 0.0f, -1.0f, 0.0f };
                PlaneFromPointNormal(plane[3], &position, &normal);

                bd->pd3dDevice->SetClipPlane(0, plane[0]);
                bd->pd3dDevice->SetClipPlane(1, plane[1]);
                bd->pd3dDevice->SetClipPlane(2, plane[2]);
                bd->pd3dDevice->SetClipPlane(3, plane[3]);
                
                const LPDIRECTDRAWSURFACE7 texture = (LPDIRECTDRAWSURFACE7)pcmd->GetTexID();
                bd->pd3dDevice->SetTexture(0, texture);

                bd->pd3dDevice->DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, bd->pVB, pcmd->VtxOffset + global_vtx_offset, cmd_list->VtxBuffer.Size, &bd->pIB[pcmd->IdxOffset + global_idx_offset], pcmd->ElemCount, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Restore the transforms
    bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &last_world);
    bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &last_view);
    bd->pd3dDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &last_projection);

    // Restore the states
    bd->pd3dDevice->ApplyStateBlock(d3d7_state_block);
    bd->pd3dDevice->DeleteStateBlock(d3d7_state_block);
}

bool ImGui_ImplDX7_Init(IDirectDraw7* ddraw, IDirect3DDevice7* device)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplDX7_Data* bd = IM_NEW(ImGui_ImplDX7_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx7";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    bd->pDD = ddraw;
    bd->pDD->AddRef();
    bd->pd3dDevice = device;
    bd->pd3dDevice->AddRef();

    return true;
}

void ImGui_ImplDX7_Shutdown()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplDX7_InvalidateDeviceObjects();
    if (bd->pd3dDevice) { bd->pd3dDevice->Release(); }
    if (bd->pDD) { bd->pDD->Release(); }
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(bd);
}

// TODO: implement this for DX7
//static bool ImGui_ImplDX7_CheckFormatSupport(IDirect3DDevice9* pDevice, D3DFORMAT format)
//{
//    IDirect3D9* pd3d = nullptr;
//    if (pDevice->GetDirect3D(&pd3d) != D3D_OK)
//        return false;
//    D3DDEVICE_CREATION_PARAMETERS param = {};
//    D3DDISPLAYMODE mode = {};
//    if (pDevice->GetCreationParameters(&param) != D3D_OK || pDevice->GetDisplayMode(0, &mode) != D3D_OK)
//    {
//        pd3d->Release();
//        return false;
//    }
//    // Font texture should support linear filter, color blend and write to render-target
//    bool support = (pd3d->CheckDeviceFormat(param.AdapterOrdinal, param.DeviceType, mode.Format, D3DUSAGE_DYNAMIC | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, D3DRTYPE_TEXTURE, format)) == D3D_OK;
//    pd3d->Release();
//    return support;
//}

static bool ImGui_ImplDX7_CreateFontsTexture()
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    unsigned char* pixels;
    int width, height, bytes_per_pixel;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);

    // Convert RGBA32 to BGRA32 (because RGBA32 is not well supported by DX9 devices)
    // TODO: implement this for DX7
//#ifndef IMGUI_USE_BGRA_PACKED_COLOR
//    const bool rgba_support = ImGui_ImplDX7_CheckFormatSupport(bd->pd3dDevice, D3DFMT_A8B8G8R8);
//    if (!rgba_support && io.Fonts->TexPixelsUseColors)
//    {
//        ImU32* dst_start = (ImU32*)ImGui::MemAlloc((size_t)width * height * bytes_per_pixel);
//        for (ImU32* src = (ImU32*)pixels, *dst = dst_start, *dst_end = dst_start + (size_t)width * height; dst < dst_end; src++, dst++)
//            *dst = IMGUI_COL_TO_DX9_ARGB(*src);
//        pixels = (unsigned char*)dst_start;
//    }
//#else
//    const bool rgba_support = false;
//#endif
    const bool rgba_support = false;

    // Upload texture to graphics system
    // Upload texture to graphics system
    bd->FontTexture = nullptr;
    DDSURFACEDESC2 font_surface_desc = { 0 };
    font_surface_desc.dwSize = sizeof(DDSURFACEDESC2);
    font_surface_desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_ALLOCONLOAD;
    font_surface_desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    font_surface_desc.dwWidth = (unsigned long)width;
    font_surface_desc.dwHeight = (unsigned long)height;
    font_surface_desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    font_surface_desc.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
    font_surface_desc.ddpfPixelFormat.dwRGBBitCount = 32;
    font_surface_desc.ddpfPixelFormat.dwRBitMask = 0x00FF0000;
    font_surface_desc.ddpfPixelFormat.dwGBitMask = 0x0000FF00;
    font_surface_desc.ddpfPixelFormat.dwBBitMask = 0x000000FF;
    font_surface_desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
    if (bd->pDD->CreateSurface(&font_surface_desc, &bd->FontTexture, NULL) != DD_OK)
    {
        return false;
    }
    if (bd->FontTexture->Lock(NULL, &font_surface_desc, DDLOCK_WAIT, NULL) != DD_OK)
    {
        return false;
    }
    for (int y = 0; y < height; ++y)
    {
        memcpy((unsigned char*)font_surface_desc.lpSurface + (size_t)font_surface_desc.lPitch * y, pixels + (size_t)width * bytes_per_pixel * y, (size_t)width * bytes_per_pixel);
    }
    if (bd->FontTexture->Unlock(NULL) != DD_OK)
    {
        return false;
    }

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->FontTexture);

    //TODO: implement this for DX7
//#ifndef IMGUI_USE_BGRA_PACKED_COLOR
//    if (!rgba_support && io.Fonts->TexPixelsUseColors)
//        ImGui::MemFree(pixels);
//#endif

    return true;
}

bool ImGui_ImplDX7_CreateDeviceObjects()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return false;
    if (!ImGui_ImplDX7_CreateFontsTexture())
        return false;
    return true;
}

void ImGui_ImplDX7_InvalidateDeviceObjects()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    if (!bd || !bd->pd3dDevice)
        return;
    if (bd->pVB) { bd->pVB->Release(); bd->pVB = nullptr; }
    if (bd->pIB) { delete[] bd->pIB; bd->pIB = nullptr; }
    if (bd->FontTexture) { bd->FontTexture->Release(); bd->FontTexture = nullptr; ImGui::GetIO().Fonts->SetTexID(0); } // We copied bd->pFontTextureView to io.Fonts->TexID so let's clear that as well.
}

void ImGui_ImplDX7_NewFrame()
{
    ImGui_ImplDX7_Data* bd = ImGui_ImplDX7_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplDX7_Init()?");

    if (!bd->FontTexture)
        ImGui_ImplDX7_CreateDeviceObjects();
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
