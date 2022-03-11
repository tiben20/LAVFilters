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


#include "stdafx.h"
#include "D3D12SwapChain.h"





struct d3d12_local_swapchain
{
    struct dxgi_swapchain* sys;

    ID3D12Device* d3d_dev;

    ID3D12CommandQueue* commandQueue;

    UINT                      m_frameIndex;

    d3d12_heap_t              m_rtvHeap; // Render Target View heap

    ID3D12Resource* m_renderTargets[DXGI_SWAP_FRAME_COUNT];
};

void CDXGISwapChain::DXGI_GetBlackColor(const d3d_format_t* pixelFormat,
    union DXGI_Color black[DXGI_MAX_RENDER_TARGET],
    size_t colors[DXGI_MAX_RENDER_TARGET])
{
    DXGI_Color blackY, blackUV, blackRGBA, blackYUY2, blackVUYA, blackY210;
    ZeroMemory(&blackY, sizeof(DXGI_Color));
    blackY.y = 0.0f;
    ZeroMemory(&blackUV, sizeof(DXGI_Color));
    blackUV.u = 0.5f;
    blackUV.v = 0.5f;
    ZeroMemory(&blackRGBA, sizeof(DXGI_Color));
    blackRGBA.r = 0.0f;
    blackRGBA.g = 0.0f;
    blackRGBA.b = 0.0f;
    blackRGBA.a = 1.0f;
    ZeroMemory(&blackYUY2, sizeof(DXGI_Color));
    blackYUY2.r = 0.0f;
    blackYUY2.g = 0.5f;
    blackYUY2.b = 0.0f;
    blackYUY2.a = 0.5f;
    ZeroMemory(&blackVUYA, sizeof(DXGI_Color));
    blackVUYA.r = 0.5f;
    blackVUYA.g = 0.5f;
    blackVUYA.b = 0.0f;
    blackVUYA.a = 1.0f;
    ZeroMemory(&blackY210, sizeof(DXGI_Color));
    blackY210.r = 0.0f;
    blackY210.g = 0.5f;
    blackY210.b = 0.5f;
    blackY210.a = 0.0f;
        
    
    static_assert(DXGI_MAX_RENDER_TARGET >= 2, "we need at least 2 RenderTargetView for NV12/P010");

    switch (pixelFormat->formatTexture)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        colors[0] = 1; black[0] = blackY;
        colors[1] = 2; black[1] = blackUV;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
        colors[0] = 4; black[0] = blackRGBA;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_YUY2:
        colors[0] = 4; black[0] = blackYUY2;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_Y410:
        colors[0] = 4; black[0] = blackVUYA;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_Y210:
        colors[0] = 4; black[0] = blackY210;
        colors[1] = 0;
        break;
    case DXGI_FORMAT_AYUV:
        colors[0] = 4; black[0] = blackVUYA;
        colors[1] = 0;
        break;
    default:
        assert(0);
    }
}

CDXGISwapChain::CDXGISwapChain(HWND hw)
{
    swapchainSurfaceType = SWAPCHAIN_SURFACE_HWND;
    hwnd = hwnd;
    //pixelFormat->formatTexture = DXGI_FORMAT_NV12;
}

CDXGISwapChain::~CDXGISwapChain()
{
}

CD3D12SwapChain::CD3D12SwapChain(ID3D12Device* d3d_dev,
    ID3D12CommandQueue* commandQueue,
    HWND hwnd)
{
    //need to reset dxgi swap here

    m_pDXGISwapChain = new CDXGISwapChain(hwnd);
    if (!!(m_pDXGISwapChain == NULL))
    {
        assert(0);
    }

    HRESULT hr = FinishInit(d3d_dev, commandQueue);
    if (FAILED(hr))
    {
        assert(0);
        m_pDXGISwapChain->DXGI_LocalSwapchainCleanupDevice();
    }

    

    

}


CD3D12SwapChain::~CD3D12SwapChain()
{
}

void CDXGISwapChain::DXGI_LocalSwapchainSwap()
{
    DXGI_PRESENT_PARAMETERS presentParams;
    ZeroMemory(&presentParams, sizeof(DXGI_PRESENT_PARAMETERS));

    HRESULT hr = dxgiswapChain->Present1(0, 0, & presentParams);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        /* TODO device lost */
        DbgLog((LOG_TRACE, 10, L"SwapChain Present failed. (hr=0x%lX)", hr));
    }
}

void CDXGISwapChain::FillSwapChainDesc(UINT width, UINT height, DXGI_SWAP_CHAIN_DESC1* out)
{
    ZeroMemory(out, sizeof(*out));
    out->BufferCount = DXGI_SWAP_FRAME_COUNT;
    out->BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    out->SampleDesc.Count = 1;
    out->SampleDesc.Quality = 0;
    out->Width = width;
    out->Height = height;
    out->Format = pixelFormat->formatTexture;
    //out->Flags = 512; // DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;

    bool isWin10OrGreater = false;
    HMODULE hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
    if (!!(hKernel32 != NULL))
        isWin10OrGreater = GetProcAddress(hKernel32, "GetSystemCpuSetInformation") != NULL;
    if (isWin10OrGreater)
        out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    else
    {
        bool isWin80OrGreater = false;
        if (!!(hKernel32 != NULL))
            isWin80OrGreater = GetProcAddress(hKernel32, "CheckTokenCapability") != NULL;
        if (isWin80OrGreater)
            out->SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        else
        {
            out->SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            out->BufferCount = 1;
        }
    }
}

void CDXGISwapChain::DXGI_CreateSwapchainHwnd(IDXGIAdapter* dxgiadapter, IUnknown* pFactoryDevice,
    UINT width, UINT height)
{
    
    assert(swapchainSurfaceType == SWAPCHAIN_SURFACE_HWND);
    if (hwnd == NULL)
    {
        DbgLog((LOG_TRACE, 10, L"missing a HWND to create the swapchain"));
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 scd;
    FillSwapChainDesc(width, height, &scd);

    IDXGIFactory2* dxgifactory;
    HRESULT hr = dxgiadapter->GetParent(IID_IDXGIFactory2, (void**)&dxgifactory);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Could not get the DXGI Factory. (hr=0x%lX))", hr));
        return;
    }

    hr = dxgifactory->CreateSwapChainForHwnd(pFactoryDevice,
        hwnd, &scd,
        NULL, NULL, &dxgiswapChain);

    if (hr == DXGI_ERROR_INVALID_CALL && scd.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        DbgLog((LOG_TRACE, 10, L"10 bits swapchain failed, try 8 bits"));
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = dxgifactory->CreateSwapChainForHwnd(pFactoryDevice,
            hwnd, &scd,
            NULL, NULL, &dxgiswapChain);
    }
    dxgifactory->Release();
    
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Could not create the SwapChain. (hr=0x%lX)", hr));
    }
}

void CDXGISwapChain::DXGI_SelectSwapchainColorspace()
{
    HRESULT hr;
    int best = 0;
    int score, best_score = 0;
    UINT support;
    IDXGISwapChain3* dxgiswapChain3 = NULL;
    hr = dxgiswapChain->QueryInterface(IID_IDXGISwapChain3, (void**)&dxgiswapChain3);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"could not get a IDXGISwapChain3"));
        goto done;
    }

    /* pick the best output based on color support and transfer */
    /* TODO support YUV output later */
    best = -1;
    for (int i = 0; color_spaces[i].name; ++i)
    {
        hr = dxgiswapChain3->CheckColorSpaceSupport(color_spaces[i].dxgi, &support);
        if (SUCCEEDED(hr) && support) {
            if (!logged_capabilities)
                DbgLog((LOG_TRACE, 10, L"supports colorspace %s", color_spaces[i].name));
            score = 0;
            if (color_spaces[i].primaries == (video_color_primaries_t)current_primaries)
                score++;
            if (color_spaces[i].color == (video_color_space_t)current_colorspace)
                score += 2; /* we don't want to translate color spaces */
            if (color_spaces[i].transfer == (video_transfer_func_t)current_transfer ||
                /* favor 2084 output for HLG source */
                (color_spaces[i].transfer == TRANSFER_FUNC_SMPTE_ST2084 && current_transfer == TRANSFER_FUNC_HLG))
                score++;
            if (color_spaces[i].b_full_range == full_range)
                score++;
            if (score > best_score || (score && best == -1)) {
                best = i;
                best_score = score;
            }
        }
    }
    logged_capabilities = true;

    if (best == -1)
    {
        best = 0;
        DbgLog((LOG_TRACE, 10, L"no matching colorspace found force %s", color_spaces[best].name));
    }

    dxgiswapChain->QueryInterface(IID_IDXGISwapChain4, (void**)&dxgiswapChain4);

    hr = dxgiswapChain3->SetColorSpace1(color_spaces[best].dxgi);
    if (SUCCEEDED(hr))
        DbgLog((LOG_TRACE, 10, L"using colorspace %s", color_spaces[best].name));
    else
        DbgLog((LOG_TRACE, 10, L"Failed to set colorspace %s. (hr=0x%lX)", color_spaces[best].name, hr));
done:
    colorspace = &color_spaces[best];
#if 0
    send_metadata = color_spaces[best].transfer == (video_transfer_func_t)cfg->transfer &&
        color_spaces[best].primaries == (video_color_primaries_t)current_primaries &&
        color_spaces[best].color == (video_color_space_t)cfg->colorspace;
#endif
    if (dxgiswapChain3)
        dxgiswapChain3->Release();
}

bool CDXGISwapChain::DXGI_UpdateSwapChain(IDXGIAdapter* dxgiadapter,
    IUnknown* pFactoryDevice,
    const d3d_format_t* newPixelFormat)
{

    if (dxgiswapChain != NULL && pixelFormat != newPixelFormat)
    {
        // the pixel format changed, we need a new swapchain
        dxgiswapChain->Release();
        dxgiswapChain = NULL;
        logged_capabilities = false;
    }

    if (dxgiswapChain == NULL)
    {
        pixelFormat = newPixelFormat;
            DXGI_CreateSwapchainHwnd(dxgiadapter, pFactoryDevice,
                width, height);

    }
    if (dxgiswapChain == NULL)
        return false;

    /* TODO detect is the size is the same as the output and switch to fullscreen mode */
    HRESULT hr;
    hr = dxgiswapChain->ResizeBuffers(0, width, height,
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Failed to resize the backbuffer. (hr=0x%lX)", hr));
        return false;
    }

    DXGI_SelectSwapchainColorspace();
    return true;
}

void CDXGISwapChain::DXGI_LocalSwapchainCleanupDevice()
{
    if (dxgiswapChain4)
    {
        dxgiswapChain4->Release();
        dxgiswapChain4 = NULL;
    }
    if (dxgiswapChain)
    {
        dxgiswapChain->Release();
        dxgiswapChain = NULL;
    }
}

HRESULT CD3D12SwapChain::CreateRenderTargets()
{
    HRESULT hr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
    
#ifndef NDEBUG
    const d3d_format_t* fmt = m_pDXGISwapChain->GetPixelFormat();
    assert(fmt->resourceFormat[0] != DXGI_FORMAT_UNKNOWN);
    assert(fmt->resourceFormat[1] == DXGI_FORMAT_UNKNOWN);
#endif
    IDXGISwapChain4* dxgiswapChain = m_pDXGISwapChain->GetSwapChain4();

    for (size_t frame = 0; frame < DXGI_SWAP_FRAME_COUNT; frame++)
    {
        hr = dxgiswapChain->GetBuffer(frame,IID_ID3D12Resource, (void**)&m_renderTargets[frame]);
        if (FAILED(hr))
            goto error;
        D3D12_ObjectSetName(m_renderTargets[frame], L"swapchain buffer[%d]", frame);

        rtvHandle = SWRenderTargetHandle(frame);
        d3d_dev->CreateRenderTargetView(m_renderTargets[frame], NULL, rtvHandle);
    }

    m_frameIndex = dxgiswapChain->GetCurrentBackBufferIndex();

    return S_OK;
error:
    return hr;
}

D3D12_CPU_DESCRIPTOR_HANDLE CD3D12SwapChain::SWRenderTargetHandle(size_t bufferIndex)
{
    return D3D12_HeapCPUHandle(&m_rtvHeap, bufferIndex);
}


bool CD3D12SwapChain::UpdateSwapchain()
{
    HRESULT hr;

    IDXGISwapChain4* dxgiswapChain = m_pDXGISwapChain->GetSwapChain4();
    if (dxgiswapChain)
    {
        D3D12_RESOURCE_DESC dsc;
        ZeroMemory(&dsc, sizeof(D3D12_RESOURCE_DESC));
        uint8_t bitsPerChannel = 0;

        if (m_renderTargets[0]) {
            dsc = m_renderTargets[0]->GetDesc();;
            assert(m_pDXGISwapChain->GetPixelFormat()->formatTexture == dsc.Format);
            bitsPerChannel = m_pDXGISwapChain->GetPixelFormat()->bitsPerChannel;
        }

        if (dsc.Width == width && dsc.Height == height && bitdepth <= bitsPerChannel)
            /* TODO also check the colorimetry */
            return true; /* nothing changed */

        for (size_t frame = 0; frame < ARRAY_SIZE(m_renderTargets); frame++)
        {
            if (m_renderTargets[frame]) {
                m_renderTargets[frame]->Release();
                m_renderTargets[frame] = NULL;
            }
        }
    }

    const d3d_format_t* newPixelFormat = NULL;

    /* favor RGB formats first */
    CD3D12Format* pFmt = new CD3D12Format();
    newPixelFormat = pFmt->FindD3D12Format(d3d_dev, 0, DXGI_RGB_FORMAT,
        bitdepth > 8 ? 10 : 8,
        0, 0,
        DXGI_CHROMA_CPU, D3D12_FORMAT_SUPPORT1_DISPLAY);
    if (!!(newPixelFormat == NULL))
        newPixelFormat = pFmt->FindD3D12Format(d3d_dev, 0, DXGI_YUV_FORMAT,
            bitdepth > 8 ? 10 : 8,
            0, 0,
            DXGI_CHROMA_CPU, D3D12_FORMAT_SUPPORT1_DISPLAY);

    if (!!(newPixelFormat == NULL)) {
        DbgLog((LOG_TRACE, 10, L"Could not get the SwapChain format."));
        return false;
    }

    if (!m_pDXGISwapChain->DXGI_UpdateSwapChain(adapter,
        (IUnknown*)commandQueue, newPixelFormat))
        return false;

    hr = CreateRenderTargets();
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Failed to create the target view. (hr=0x%lX)", hr));
        return false;
    }

    m_pDXGISwapChain->DXGI_LocalSwapchainSwap();

    return true;
}

void CD3D12SwapChain::D3D12_LocalSwapchainCleanupDevice()
{
    

    for (size_t frame = 0; frame < DXGI_SWAP_FRAME_COUNT; frame++)
    {
        if (!m_renderTargets[frame])
            continue;
        m_renderTargets[frame]->Release();
        m_renderTargets[frame] = NULL;
    }
    ((d3d12_heap_t*)&m_rtvHeap)->heap->Release();

    m_pDXGISwapChain->DXGI_LocalSwapchainCleanupDevice();
}

bool D3D12_LocalSwapchainUpdateOutput(void* opaque)
{
    /*
    struct d3d12_local_swapchain* display = opaque;
    if (!UpdateSwapchain(display, cfg))
        return false;
    DXGI_SwapchainUpdateOutput(m_pDXGISwapChain, out);*/
    return true;
}

void CD3D12SwapChain::D3D12_LocalSwapchainSwap()
{
    m_pDXGISwapChain->DXGI_LocalSwapchainSwap();
}

void D3D12_LocalSwapchainSetMetadata(void* opaque, const void* metadata)
{
    //only libvlc_video_metadata_frame_hdr10 in the type
    /*struct d3d12_local_swapchain* display = opaque;
    DXGI_LocalSwapchainSetMetadata(m_pDXGISwapChain, type, metadata);*/
}

bool CD3D12SwapChain::D3D12_LocalSwapchainStartEndRendering(void* opaque, bool enter)
{
    if (enter)
    {
        IDXGISwapChain4* dxgiswapChain = m_pDXGISwapChain->GetSwapChain4();
        m_frameIndex = dxgiswapChain->GetCurrentBackBufferIndex();
    }
    return true;
}

bool CD3D12SwapChain::D3D12_LocalSwapchainSelectPlane(void* opaque, size_t plane, void* out)
{
    
    if (!m_renderTargets[m_frameIndex])
        return false;
    if (plane > 1)
        return false; // not supported

    struct vlc_d3d12_plane* params = (struct vlc_d3d12_plane*)out;
    params->renderTarget = m_renderTargets[m_frameIndex];
    params->descriptor = SWRenderTargetHandle(m_frameIndex);

    size_t colorCount[DXGI_MAX_RENDER_TARGET];
    union DXGI_Color black[DXGI_MAX_RENDER_TARGET];
    m_pDXGISwapChain->DXGI_GetBlackColor(m_pDXGISwapChain->GetPixelFormat(), black, colorCount);
    if (!!(colorCount[plane] == 0))
        return false;
    params->background = black[plane];

#if !defined(NDEBUG) && 0
    switch (m_frameIndex)
    {
    case 0: params->black.r = 1.0f; break;
    case 1: params->black.g = 1.0f; break;
    case 2: params->black.b = 1.0f; break;
    }
#endif
    return true;
}

HRESULT CD3D12SwapChain::FinishInit(ID3D12Device* pd3d_dev,
    ID3D12CommandQueue* cmqqueue)
{
    d3d_dev = pd3d_dev;
    commandQueue = cmqqueue;

    HRESULT hr;
    hr = D3D12_HeapInit(d3d_dev, &m_rtvHeap,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        DXGI_SWAP_FRAME_COUNT);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"Failed to create the swapchain heap. (hr=0x%lX)", hr));
        return hr;
    }

    return S_OK;
}


