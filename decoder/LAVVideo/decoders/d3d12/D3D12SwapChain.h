/*
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#pragma once

#define DXGI_SWAP_FRAME_COUNT   3

#include "d3d12.h"
#include "D3D12Format.h"
#include "d3d12shaders.h"
#include "LAVPixFmtConverter.h"
#include "dxgi1_5.h"
typedef enum video_color_axis {
    COLOR_AXIS_RGB,
    COLOR_AXIS_YCBCR,
} video_color_axis;

typedef enum swapchain_surface_type {
    SWAPCHAIN_SURFACE_HWND,
    SWAPCHAIN_SURFACE_DCOMP,
} swapchain_surface_type;

typedef struct {
    DXGI_COLOR_SPACE_TYPE   dxgi;
    const char* name;
    video_color_axis        axis;
    video_color_primaries_t primaries;
    video_transfer_func_t   transfer;
    video_color_space_t     color;
    bool                    b_full_range;
} dxgi_color_space;

DEFINE_GUID(GUID_SWAPCHAIN_WIDTH, 0xf1b59347, 0x1643, 0x411a, 0xad, 0x6b, 0xc7, 0x80, 0x17, 0x7a, 0x06, 0xb6);
DEFINE_GUID(GUID_SWAPCHAIN_HEIGHT, 0x6ea976a0, 0x9d60, 0x4bb7, 0xa5, 0xa9, 0x7d, 0xd1, 0x18, 0x7f, 0xc9, 0xbd);

#define DXGI_COLOR_RANGE_FULL   1 /* 0-255 */
#define DXGI_COLOR_RANGE_STUDIO 0 /* 16-235 */

#define TRANSFER_FUNC_10    TRANSFER_FUNC_LINEAR
#define TRANSFER_FUNC_22    TRANSFER_FUNC_SRGB
#define TRANSFER_FUNC_2084  TRANSFER_FUNC_SMPTE_ST2084

#define COLOR_PRIMARIES_BT601  COLOR_PRIMARIES_BT601_525

static const dxgi_color_space color_spaces[] = {
#define DXGIMAP(AXIS, RANGE, GAMMA, SITTING, PRIMARIES) \
    { DXGI_COLOR_SPACE_##AXIS##_##RANGE##_G##GAMMA##_##SITTING##_P##PRIMARIES, \
      #AXIS " Rec." #PRIMARIES " gamma:" #GAMMA " range:" #RANGE, \
      COLOR_AXIS_##AXIS, COLOR_PRIMARIES_BT##PRIMARIES, TRANSFER_FUNC_##GAMMA, \
      COLOR_SPACE_BT##PRIMARIES, DXGI_COLOR_RANGE_##RANGE},

    DXGIMAP(RGB,   FULL,     22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   601)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   601)
    DXGIMAP(RGB,   FULL,     10,    NONE,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,   709)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,   709)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,   709)
    DXGIMAP(RGB,   STUDIO,   22,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO,   22,    LEFT,  2020)
    DXGIMAP(YCBCR, FULL,     22,    LEFT,  2020)
    DXGIMAP(YCBCR, STUDIO,   22, TOPLEFT,  2020)
    DXGIMAP(RGB,   FULL,     22,    NONE,  2020)
    DXGIMAP(RGB,   FULL,   2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084,    LEFT,  2020)
    DXGIMAP(RGB,   STUDIO, 2084,    NONE,  2020)
    DXGIMAP(YCBCR, STUDIO, 2084, TOPLEFT,  2020)
    DXGIMAP(YCBCR, STUDIO,  HLG, TOPLEFT,  2020)
    DXGIMAP(YCBCR, FULL,    HLG, TOPLEFT,  2020)
    /*DXGIMAP(YCBCR, FULL,     22,    NONE,  2020, 601)*/
    
#undef DXGIMAP
};

#ifdef HAVE_DXGI1_6_H
static bool canHandleConversion(const dxgi_color_space* src, const dxgi_color_space* dst)
{
    if (src == dst)
        return true;
    if (src->primaries == COLOR_PRIMARIES_BT2020)
        return true; /* we can convert BT2020 to 2020 or 709 */
    if (dst->transfer == TRANSFER_FUNC_BT709)
        return true; /* we can handle anything to 709 */
    return false; /* let Windows do the rest */
}
#endif

static void D3D12_FenceWaitForPreviousFrame(d3d12_fence_t* fence, ID3D12CommandQueue* commandQueue)
{
    if (fence->fenceCounter == UINT64_MAX)
        fence->fenceCounter = 0;
    else
        fence->fenceCounter++;

    ResetEvent(fence->fenceReached);
    fence->d3dRenderFence->SetEventOnCompletion(fence->fenceCounter, fence->fenceReached);
    commandQueue->Signal(fence->d3dRenderFence, fence->fenceCounter);

    WaitForSingleObject(fence->fenceReached, INFINITE);
}

class CDXGISwapChain
{
public:
    CDXGISwapChain(HWND hw);
    ~CDXGISwapChain();
    IDXGISwapChain4* GetSwapChain4() {
        return dxgiswapChain4;
    };
    const d3d_format_t* GetPixelFormat() {
        return pixelFormat;
    };
    bool DXGI_UpdateSwapChain(IDXGIAdapter* dxgiadapter,
        IUnknown* pFactoryDevice,
        const d3d_format_t* newPixelFormat);
    void DXGI_CreateSwapchainHwnd(IDXGIAdapter* dxgiadapter, IUnknown* pFactoryDevice,
        UINT width, UINT height);

    void DXGI_LocalSwapchainSwap();
    void DXGI_GetBlackColor(const d3d_format_t* pixelFormat,
        union DXGI_Color black[DXGI_MAX_RENDER_TARGET],
        size_t colors[DXGI_MAX_RENDER_TARGET]);
    void FillSwapChainDesc(UINT width, UINT height, DXGI_SWAP_CHAIN_DESC1* out);
    void DXGI_LocalSwapchainCleanupDevice();
    void DXGI_SelectSwapchainColorspace();

private:
    video_color_primaries_t current_primaries;
    video_color_space_t current_colorspace;
    video_transfer_func_t current_transfer;
    bool                    full_range;
    int height;
    int width;
    int bitdepth;
    const d3d_format_t* pixelFormat;
    const dxgi_color_space* colorspace;

    swapchain_surface_type  swapchainSurfaceType;

    HWND hwnd;
    
    IDXGISwapChain1* dxgiswapChain;   /* DXGI 1.2 swap chain */
    IDXGISwapChain4* dxgiswapChain4;  /* DXGI 1.5 for HDR metadata */
    bool                    send_metadata;
    DXGI_HDR_METADATA_HDR10 hdr10;

    bool                   logged_capabilities;
protected:

};

class CD3D12SwapChain
{
public:
    CD3D12SwapChain(ID3D12Device* d3d_dev,ID3D12CommandQueue* commandQueue,HWND hwnd);
    ~CD3D12SwapChain();
    HRESULT CreateRenderTargets();
    bool UpdateSwapchain();
    D3D12_CPU_DESCRIPTOR_HANDLE SWRenderTargetHandle(size_t bufferIndex);
    void D3D12_LocalSwapchainSwap();
    bool D3D12_LocalSwapchainStartEndRendering(void* opaque, bool enter);
    //static bool D3D12_LocalSwapchainSelectPlane(void* opaque, size_t plane, void* out);
    bool D3D12_LocalSwapchainSelectPlane(void* opaque, size_t plane, void* out);
    void D3D12_LocalSwapchainCleanupDevice();
    void* D3D12_CreateLocalSwapchainHandleHwnd(ID3D12Device* d3d_dev,
        ID3D12CommandQueue* commandQueue,
        HWND hwnd);
    HRESULT FinishInit(ID3D12Device* d3d_dev,
        ID3D12CommandQueue* commandQueue);
    
private:
    int height;
    int width;
    int bitdepth;
    CDXGISwapChain *m_pDXGISwapChain;
    ID3D12Device* d3d_dev;
    IDXGIAdapter* adapter;
    ID3D12CommandQueue* commandQueue;

    UINT                      m_frameIndex;

    d3d12_heap_t              m_rtvHeap; // Render Target View heap

    ID3D12Resource* m_renderTargets[DXGI_SWAP_FRAME_COUNT];
protected:

};

