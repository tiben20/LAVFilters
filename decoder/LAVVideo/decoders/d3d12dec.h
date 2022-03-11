/*
 *      Copyright (C) 2017-2021 Hendrik Leppkes
 *      http://www.1f0.de
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
#include "DecBase.h"
#include "avcodec.h"

#include <d3d12.h>
#include <d3d12video.h>
#include <dxgi1_3.h>
#include <vector>
#include "d3d12/D3D12Commands.h"
#include "d3d12/D3D12Shaders.h"
#include "d3d12/D3D12Quad.h"
#include "d3d12/D3D12SwapChain.h"
/*comptr*/
#define MAX_SURFACE_COUNT (64)
#include "d3d12/D3D12SurfaceAllocator.h"
extern "C"
{
#include "libavutil/hwcontext.h"
#include "libavcodec/d3d12va.h"
    #include "libavutil/imgutils.h"
}
typedef bool(*libvlc_video_output_select_plane_cb)(void* opaque, size_t plane, void* output);

typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY1)(REFIID riid, void **ppFactory);




typedef struct plane_t
{
    uint8_t* p_pixels;                        /**< Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;           /**< Number of lines, including margins */
    int i_pitch;           /**< Number of bytes in a line, including margins */

    /** Size of a macropixel, defaults to 1 */
    int i_pixel_pitch;

    /* Variables used for pictures with margins */
    int i_visible_lines;            /**< How many visible lines are there? */
    int i_visible_pitch;            /**< How many visible pixels are there? */

} plane_t;

typedef struct {
    ID3D12Resource* res;
    D3D12_HEAP_PROPERTIES heap;
    int index;
}D3DResource12;
class ResourcePool
{
public:
    ResourcePool(ID3D12Device* pDev)
    {
        m_pDevice = pDev;
        pDev->AddRef();
    }
    ~ResourcePool()
    {
        SafeRelease(&m_pDevice);
        thepool.clear();
        /*for (std::vector<D3DResource12*>::iterator it = thepool.begin(); it != thepool.end(); it++)
        {
            

        }*/
    }

    int AddResource(ID3D12Resource* res, D3D12_HEAP_PROPERTIES heap)
    {
        D3DResource12* newvec = new D3DResource12();
        newvec->heap = heap;
        newvec->res = res;
        newvec->index = thepool.size();
        thepool.push_back(newvec);
    }
private:
    ID3D12Device* m_pDevice;
    std::vector<D3DResource12*> thepool;
};
//todo surface pool
class CDecD3D12 : public CDecAvcodec
{
  public:
    CDecD3D12(void);
    virtual ~CDecD3D12(void);

    // ILAVDecoder
    STDMETHODIMP Check();
    STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
    STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp);
    STDMETHODIMP Flush();
    STDMETHODIMP EndOfStream();

    STDMETHODIMP InitAllocator(IMemAllocator **ppAlloc);
    STDMETHODIMP PostConnect(IPin *pPin);
    STDMETHODIMP BreakConnect();
    STDMETHODIMP_(long) GetBufferCount(long *pMaxBuffers = nullptr);
    STDMETHODIMP_(const WCHAR *) GetDecoderName()
    {
        return m_bReadBackFallback ? (m_bDirect ? L"D3D12 cb direct" : L"D3D12 cb") : L"D3D12 native";
    }
    STDMETHODIMP HasThreadSafeBuffers() { return S_FALSE; }
    STDMETHODIMP SetDirectOutput(BOOL bDirect)
    {
        m_bDirect = bDirect;
        return S_OK;
    }
    STDMETHODIMP_(DWORD) GetHWAccelNumDevices();
    STDMETHODIMP GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier);
    STDMETHODIMP GetHWAccelActiveDevice(BSTR *pstrDeviceName);

    // CDecBase
    STDMETHODIMP Init();

    void free_buffer(int idx);
  protected:
    HRESULT AdditionaDecoderInit();
    HRESULT PostDecode();

    HRESULT HandleDXVA2Frame(LAVFrame *pFrame);
    HRESULT DeliverD3D12Frame(LAVFrame *pFrame);
    HRESULT DeliverD3D12Readback(LAVFrame *pFrame);
    HRESULT DeliverD3D12ReadbackDirect(LAVFrame *pFrame);

    BOOL IsHardwareAccelerator() { return TRUE; }


    
  private:
    STDMETHODIMP DestroyDecoder(bool bFull);

    STDMETHODIMP ReInitD3D12Decoder(AVCodecContext *s);

    STDMETHODIMP CreateD3D12Device(UINT nDeviceIndex);

    STDMETHODIMP CreateD3D12Decoder();
    STDMETHODIMP AllocateFramesContext(int width, int height, AVPixelFormat format, int nSurfaces,
                                       AVBufferRef **pFramesCtx);

    STDMETHODIMP FillHWContext(AVD3D12VAContext *ctx);

    STDMETHODIMP FlushDisplayQueue(BOOL bDeliver);

    static enum AVPixelFormat get_d3d12_format(struct AVCodecContext *s, const enum AVPixelFormat *pix_fmts);
    static int get_d3d12_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
    
    static void log_callback_null(void *ptr, int level, const char *fmt, va_list vl);
    STDMETHODIMP UpdateStaging();
    STDMETHODIMP UpdateStaging(int index);
    /*command and fence*/
    
  private:
    CD3D12Commands* m_pD3DCommands;
    ID3D12Debug* m_pD3DDebug;
    ID3D12Debug1* m_pD3DDebug1;
    ID3D12Device* m_pD3DDevice;
    ID3D12VideoDevice* m_pVideoDevice;
    D3D12_VIDEO_DECODER_HEAP_DESC m_pVideoDecoderCfg;

    const d3d_format_t* render_fmt;
    video_format_t texture_fmt;
    display_info_t displayFormat;
    ID3D12VideoDecoder* m_pVideoDecoder;
    //ResourcePool* m_pResourcePool;

    const d3d_mode_t* m_pRenderFormat;
    CD3D12Format* pFormat;
    CD3D12Shaders* pShaders;
    CD3D12SwapChain* pSwapChain;
    libvlc_video_output_select_plane_cb      selectPlaneCb;
    /* avcodec internals */
    struct AVD3D12VAContext m_pD3D12VAContext;

    ID3D12Resource*                 m_pTexture[MAX_SURFACE_COUNT];//texture array
    ID3D12Resource*                 ref_table[MAX_SURFACE_COUNT];//ref frame
    UINT                            ref_index[MAX_SURFACE_COUNT];//ref frame index
    //bool                            ref_texture_used[MAX_SURFACE_COUNT];//ref frame index
    std::list<int>                ref_free_index;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT ouput_footprint[32];
    UINT64 output_pitch[32];
    UINT output_rows[32];
    UINT64 output_requiredsize;

    int curindex;
    /*start rendering*/
    /*new Staging*/
    ID3D12Resource* m_pStagingTexture;
    D3D12_HEAP_PROPERTIES m_pStagingProp;
    D3D12_RESOURCE_DESC m_pStagingDesc;
    d3d12_footprint_t m_pStagingLayout;
    /**/
    /* Sensors */
    void* p_sensors;

    AVFrame* m_pFrame = nullptr;

    //d3d12_device_t* d3d_dev;

    d3d_shader_compiler_t    shaders;
    CD3D12Quad             *picQuad;

    d3d12_fence_t            renderFence;

    ID3D12CommandQueue* m_commandQueue;

    //d3d12_command_list_t     commandLists;
    std::vector<ID3D12CommandList*> commandLists;
    d3d12_footprint_t layout_out[2];
    d3d12_footprint_t layout_in[2];
    struct {
        picture_sys_d3d12_t  picSys;
        d3d12_footprint_t    m_textureFootprint[DXGI_MAX_SHADER_VIEW];

        ID3D12Resource* textureUpload[DXGI_MAX_SHADER_VIEW];
        //max plane 5
        plane_t              planes[5];

        d3d12_commands_t     commandList;
    } staging;

    // SPU
    //vlc_fourcc_t             pSubpictureChromas[2];
    uint32_t pSubpictureChromas[2];
    CD3D12Quad             *regionQuad;
    int                      d3dregion_count;
    //picture_t** d3dregions;
    //vlc picture_t
    HANDLE render_lock;
    //vlc_mutex_t              render_lock;
    /*end rendering*/
    //CD3D12SurfaceAllocator* m_pAllocator = nullptr;
    
    IDXGIAdapter1 *m_pDxgiAdapter;
    IDXGIFactory2 *m_pDxgiFactory;

    DWORD m_dwSurfaceWidth = 0;
    DWORD m_dwSurfaceHeight = 0;
    DWORD m_dwSurfaceCount = 0;
    DXGI_FORMAT m_SurfaceFormat = DXGI_FORMAT_UNKNOWN;


    BOOL m_bReadBackFallback = FALSE;
    BOOL m_bDirect = FALSE;
    BOOL m_bFailHWDecode = FALSE;

    static void ReleaseFrame12(void* opaque, uint8_t* data);
    
    //LAVFrame* m_FrameQueue[4];

    int m_FrameQueuePosition = 0;
    int m_DisplayDelay = 4;



    DXGI_ADAPTER_DESC m_AdapterDesc = {0};
    friend class CD3D12SurfaceAllocator;
};
