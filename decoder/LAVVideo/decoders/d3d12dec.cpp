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

#include "stdafx.h"
#include "d3d12dec.h"
//#include "ID3DVideoMemoryConfiguration.h"
#include "dxva2/dxva_common.h"
#include <d3d12.h>
#include <d3d12video.h>
#include "d3dx12.h"


extern "C"
{
#include "libavutil/hwcontext.h"
}

#define ROUND_UP_128(num) (((num) + 127) & ~127)


ILAVDecoder *CreateDecoderD3D12()
{
    return new CDecD3D12();
}

STDMETHODIMP VerifyD3D12Device(DWORD &dwIndex, DWORD dwDeviceId)
{
    HRESULT hr = S_OK;
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// D3D12 decoder implementation
////////////////////////////////////////////////////////////////////////////////


CDecD3D12::CDecD3D12(void)
    : CDecAvcodec()
{
    m_pD3DCommands = nullptr;
    
    m_pD3DDevice = nullptr;
    m_pVideoDevice = nullptr;
    m_pDxgiAdapter = nullptr;
    m_pDxgiFactory = nullptr;
    m_pVideoDecoder = nullptr;
    m_pD3DDebug = nullptr;
    m_pD3DDebug1 = nullptr;
    for (int x = 0; x < MAX_SURFACE_COUNT; x++)
        m_pTexture[x] = nullptr;

    m_pStagingTexture = nullptr;
    av_frame_free(&m_pFrame);
    m_pD3D12VAContext.decoder = nullptr;
}

CDecD3D12::~CDecD3D12(void)
{
    DestroyDecoder(true);

    
}

STDMETHODIMP CDecD3D12::DestroyDecoder(bool bFull)
{
   
    
    m_pD3DCommands = nullptr;
    SafeRelease(&m_pD3DDebug);
    SafeRelease(&m_pDxgiAdapter);
    SafeRelease(&m_pD3DDebug1);
    SafeRelease(&m_pD3DDebug);
    SafeRelease(&m_pStagingTexture);
    for (int x = 0; x < MAX_SURFACE_COUNT; x++)
    {
        SafeRelease(&m_pTexture[x]);

    }

    SafeRelease(&m_pVideoDevice);
    SafeRelease(&m_pDxgiAdapter);
    SafeRelease(&m_pDxgiFactory);
    SafeRelease(&m_pD3DDebug);
    SafeRelease(&m_pD3DDevice);
    SafeRelease(&m_pVideoDecoder);
    
    av_frame_free(&m_pFrame);
    
    SafeRelease(&m_pD3D12VAContext.decoder);
    
    CDecAvcodec::DestroyDecoder();
    
    return S_OK;
}

// ILAVDecoder
STDMETHODIMP CDecD3D12::Init()
{
    return S_OK;
}

STDMETHODIMP CDecD3D12::Check()
{
    HRESULT hr = S_OK;
    return hr;
}

STDMETHODIMP CDecD3D12::InitAllocator(IMemAllocator **ppAlloc)
{
    HRESULT hr = S_OK;
    if (m_bReadBackFallback)
        return E_NOTIMPL;

    return E_NOTIMPL;
}

STDMETHODIMP CDecD3D12::CreateD3D12Device(UINT nDeviceIndex)
{
    HRESULT hr = S_OK;
    
    // ComPtr<ID3D12VideoDecoder> decoder;
    // ComPtr<ID3D12VideoDevice> videodevice12;
    // create DXGI factory
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_pD3DDebug))))
    {
        m_pD3DDebug->EnableDebugLayer();
        m_pD3DDebug->QueryInterface(IID_PPV_ARGS(&m_pD3DDebug1));
        //m_pD3DDebug1->SetEnableGPUBasedValidation(true);
        m_pD3DDebug1->SetEnableSynchronizedCommandQueueValidation(1);
        
        

    }
    hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pDxgiFactory));
    if FAILED (hr)
        assert(0);

    if (DXGI_ERROR_NOT_FOUND == m_pDxgiFactory->EnumAdapters1(0, &m_pDxgiAdapter))
    {
        hr = S_FALSE;
        assert(0);
    }
    DXGI_ADAPTER_DESC1 desc;
    hr = m_pDxgiAdapter->GetDesc1(&desc);
    if FAILED (hr)
        assert(0);
    const D3D_FEATURE_LEVEL *levels = NULL;
    D3D_FEATURE_LEVEL max_level = D3D_FEATURE_LEVEL_12_1;
    //int level_count = s_GetD3D12FeatureLevels(max_level, &levels);

    hr = D3D12CreateDevice(m_pDxgiAdapter, max_level, IID_PPV_ARGS(&m_pD3DDevice));
    if FAILED (hr)
        assert(0);
    
    

    hr = m_pD3DDevice->QueryInterface(IID_PPV_ARGS(&m_pVideoDevice));
    return hr;
}



static const D3D_FEATURE_LEVEL s_D3D12Levels[] = {
    D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
};

static int s_GetD3D12FeatureLevels(int max_fl, const D3D_FEATURE_LEVEL **out)
{
    static const int levels_len = countof(s_D3D12Levels);

    int start = 0;
    for (; start < levels_len; start++)
    {
        if (s_D3D12Levels[start] <= max_fl)
            break;
    }
    *out = &s_D3D12Levels[start];
    return levels_len - start;
}

STDMETHODIMP CDecD3D12::PostConnect(IPin *pPin)
{

    DbgLog((LOG_TRACE, 10, L"CDecD3D12::PostConnect()"));
    HRESULT hr = S_OK;
    m_pCallback->ReleaseAllDXVAResources();

    
    //UINT nDevice = LAVHWACCEL_DEVICE_DEFAULT;
    UINT nDevice = m_pSettings->GetHWAccelDeviceIndex(HWAccel_D3D11, nullptr);
    if (!m_pD3DDevice)
    {
        hr = CreateD3D12Device(nDevice);
    }


    m_bReadBackFallback = true;
    return S_OK;
fail:
    return E_FAIL;
}

STDMETHODIMP CDecD3D12::BreakConnect()
{
    if (m_bReadBackFallback)
        return S_FALSE;

    // release any resources held by the core
    m_pCallback->ReleaseAllDXVAResources();

    // flush all buffers out of the decoder to ensure the allocator can be properly de-allocated
    if (m_pAVCtx && avcodec_is_open(m_pAVCtx))
        avcodec_flush_buffers(m_pAVCtx);

    return S_OK;
}

void CDecD3D12::log_callback_null(void *ptr, int level, const char *fmt, va_list vl)
{
    char out[4096];
    vsnprintf(out, sizeof(out), fmt, vl);
    DbgLog((LOG_TRACE, 10, out));
    
}

STDMETHODIMP CDecD3D12::InitDecoder(AVCodecID codec, const CMediaType *pmt)
{
    HRESULT hr = S_OK;
    DbgLog((LOG_TRACE, 10, L"CDecD3D12::InitDecoder(): Initializing D3D12 decoder"));

    // Destroy old decoder
    DestroyDecoder(false);

    // reset stream compatibility
    m_bFailHWDecode = false;

    

    hr = CDecAvcodec::InitDecoder(codec, pmt);
    //m_pAVCtx->pix_fmt = AV_PIX_FMT_D3D12;
    m_pAVCtx->debug = 0;
    av_log_set_level(AV_LOG_ERROR);
    av_log_set_callback(log_callback_null);

    if (FAILED(hr))
        return hr;


    // initialize surface format to ensure the default media type is set properly
    //d3d11va_map_sw_to_hw_format(m_pAVCtx->sw_pix_fmt); the surface is always nv12 d3d11 can't always do nv12 but d3d12 can
    m_SurfaceFormat = DXGI_FORMAT_NV12;
    m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
    m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);

    return S_OK;
}

HRESULT CDecD3D12::AdditionaDecoderInit()
{
    AVD3D12VAContext *ctx = av_d3d12va_alloc_context();
    if (!m_pD3DDevice)
    {
        CreateD3D12Device(0);
    }
    if (m_pVideoDecoder)
    {
        FillHWContext(ctx);
        m_pAVCtx->hwaccel_context = ctx;
    }

    //TODO FIX FORMAT
    m_pAVCtx->thread_count = 1;
    m_pAVCtx->get_format = get_d3d12_format;
    m_pAVCtx->get_buffer2 = get_d3d12_buffer;
    m_pAVCtx->opaque = this;
    m_pAVCtx->slice_flags |= SLICE_FLAG_ALLOW_FIELD;
    // disable error concealment in hwaccel mode, it doesn't work either way
    m_pAVCtx->error_concealment = 0;
    av_opt_set_int(m_pAVCtx, "enable_er", 0, AV_OPT_SEARCH_CHILDREN);
    // ctx->test_buffer = TestBuffer;
    return S_OK;
}

enum AVPixelFormat CDecD3D12::get_d3d12_format(struct AVCodecContext *s, const enum AVPixelFormat *pix_fmts)
{
    CDecD3D12 *pDec = (CDecD3D12 *)s->opaque;
    HRESULT hr = pDec->ReInitD3D12Decoder(s);

    return AV_PIX_FMT_D3D12_VLD;
}
void CDecD3D12::ReleaseFrame12(void* opaque, uint8_t* data)
{
    (void)data;

    //LAVFrame* pFrame = (LAVFrame*)opaque;

#if 0
    if (pFrame->destruct)
    {
        pFrame->destruct(pFrame);
        pFrame->destruct = nullptr;
        pFrame->priv_data = nullptr;
    }

    memset(pFrame->data, 0, sizeof(pFrame->data));
    memset(pFrame->stereo, 0, sizeof(pFrame->stereo));
    memset(pFrame->stride, 0, sizeof(pFrame->stride));

    for (int i = 0; i < pFrame->side_data_count; i++)
    {
        SAFE_CO_FREE(pFrame->side_data[i].data);
    }
    SAFE_CO_FREE(pFrame->side_data);

    pFrame->side_data_count = 0;
#endif
    //SAFE_CO_FREE(pFrame);

    
}

int CDecD3D12::get_d3d12_buffer(struct AVCodecContext *c, AVFrame *frame, int flags)
{
    CDecD3D12 *pDec = (CDecD3D12 *)c->opaque;

    frame->opaque = NULL;
    
    
    UINT freeindex=-1;
    if (pDec->ref_free_index.size() == 0)
        return -1;

    frame->buf[0] = av_buffer_create(frame->data[0], 0, ReleaseFrame12, nullptr, 0);
    frame->data[0] = (uint8_t*)pDec->m_pD3D12VAContext.surfaces.NumTexture2Ds;//array size
    frame->data[3] = (uint8_t*)pDec->ref_free_index.front();//output index of the frame
    //pDec->ref_free_index.front() = std::move(pDec->ref_free_index.back());
    pDec->ref_free_index.pop_front();
    //frame->opaque = this;
    return 0;
    //&pDec->m_pTexture;
    
    /*if (pDec->m_pFramesCtx)
    {
        int ret = av_hwframe_get_buffer(pDec->m_pFramesCtx, frame, 0);
        frame->data[2] = (uint8_t *) 8;
        frame->width = c->coded_width;
        frame->height = c->coded_height;
        return ret;
    }*/
    
    return 0;
}





STDMETHODIMP CDecD3D12::FillHWContext(AVD3D12VAContext *ctx)
{
    
    ctx->decoder = m_pVideoDecoder;
    
    return S_OK;
}

STDMETHODIMP_(long) CDecD3D12::GetBufferCount(long *pMaxBuffers)
{
    //to do add check for each codec but for h264 its already 16
    long buffers = 16;

    return buffers;
}

STDMETHODIMP CDecD3D12::FlushDisplayQueue(BOOL bDeliver)
{
    /*for (int i = 0; i < m_DisplayDelay; ++i)
    {
        if (m_FrameQueue[m_FrameQueuePosition])
        {
            if (bDeliver)
            {
                DeliverD3D12Frame(m_FrameQueue[m_FrameQueuePosition]);
                m_FrameQueue[m_FrameQueuePosition] = nullptr;
            }
            else
            {
                //ReleaseFrame(&m_FrameQueue[m_FrameQueuePosition]);
            }
        }
        m_FrameQueuePosition = (m_FrameQueuePosition + 1) % m_DisplayDelay;
    }*/

    return S_OK;
    DbgLog((LOG_TRACE, 10, L"FlushDisplayQueue"));
    return S_OK;
}

STDMETHODIMP CDecD3D12::Flush()
{
/*
    CDecAvcodec::Flush();
    */
    // Flush display queue
    FlushDisplayQueue(FALSE);

    return S_OK;
}

STDMETHODIMP CDecD3D12::EndOfStream()
{
    CDecAvcodec::EndOfStream();

    // Flush display queue
    FlushDisplayQueue(TRUE);

    return S_OK;
}

HRESULT CDecD3D12::PostDecode()
{
    if (m_bFailHWDecode)
    {
        DbgLog((LOG_TRACE, 10, L"::PostDecode(): HW Decoder failed, falling back to software decoding"));
        return E_FAIL;
    }
    return S_OK;
}


STDMETHODIMP CDecD3D12::ReInitD3D12Decoder(AVCodecContext* s)
{
    CDecD3D12* pDec = (CDecD3D12*)s->opaque;
    HRESULT hr = S_OK;
    // Don't allow decoder creation during first init
    if (m_bInInit)
        return S_FALSE;

    D3D12_VIDEO_DECODER_DESC decoderDesc;
    CD3D12Format* fmt = new CD3D12Format();

    //D3D12_OPAQUE
    //bits 8 width and height 2
    //D3D12_OPAQUE_BGRA
    //bits 8 width and height 1

    const d3d_format_t* processorInput[4];
    int idx = 0;
    const d3d_format_t* decoder_format;
    int bitdepth = 8;
    int chromah, chromaw;
    UINT supportFlags = D3D12_FORMAT_SUPPORT1_DECODER_OUTPUT | D3D12_FORMAT_SUPPORT1_SHADER_LOAD;
    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
            chromah = chromaw = 2;
        else if (i == 1)
            chromah = chromaw = 1;
        decoder_format = fmt->FindD3D12Format(pDec->m_pD3DDevice, 0, DXGI_RGB_FORMAT | DXGI_YUV_FORMAT,
            bitdepth, chromah + 1, chromaw + 1,
            DXGI_CHROMA_GPU, supportFlags);
        if (decoder_format == NULL)
            decoder_format = fmt->FindD3D12Format(pDec->m_pD3DDevice, 0, DXGI_RGB_FORMAT | DXGI_YUV_FORMAT,
                bitdepth, 0, 0, DXGI_CHROMA_GPU, supportFlags);
        if (decoder_format == NULL && bitdepth > 10)
            decoder_format = fmt->FindD3D12Format(pDec->m_pD3DDevice, 0, DXGI_RGB_FORMAT | DXGI_YUV_FORMAT,
                10, 0, 0, DXGI_CHROMA_GPU, supportFlags);
        if (decoder_format == NULL)
            decoder_format = fmt->FindD3D12Format(pDec->m_pD3DDevice, 0, DXGI_RGB_FORMAT | DXGI_YUV_FORMAT,
                0, 0, 0, DXGI_CHROMA_GPU, supportFlags);
        if (decoder_format != NULL)
        {
            processorInput[idx++] = decoder_format;
        }

    }
    if (decoder_format == NULL || decoder_format->formatTexture != DXGI_FORMAT_NV12)
        processorInput[idx++] = fmt->D3D12_FindDXGIFormat(DXGI_FORMAT_NV12);
    processorInput[idx++] = fmt->D3D12_FindDXGIFormat(DXGI_FORMAT_420_OPAQUE);

    D3D12_FEATURE_DATA_VIDEO_DECODE_FORMAT_COUNT decode_formats;
    decode_formats.NodeIndex = 0;
    decode_formats.FormatCount = 0;
    decode_formats.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
    decode_formats.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
    decode_formats.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
    hr = pDec->m_pVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_FORMAT_COUNT, &decode_formats, sizeof(decode_formats));
    if (FAILED(hr) || decode_formats.FormatCount == 0)
    {

        return S_FALSE;
    }

    //std::vector<DXGI_FORMAT> supported_formats;
    DXGI_FORMAT supported_formats[10];
    D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS formats;
    formats.NodeIndex = 0;
    formats.Configuration = decode_formats.Configuration;
    formats.FormatCount = decode_formats.FormatCount;
    formats.pOutputFormats = supported_formats;

    hr = pDec->m_pVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_FORMATS, &formats, sizeof(formats));
    if (FAILED(hr))
    {
        return S_FALSE;
    }
    for (idx = 0; idx < 4; ++idx)
    {
        BOOL is_supported = false;
        for (size_t f = 0; f < decode_formats.FormatCount; f++)
        {
            if (supported_formats[f] == processorInput[idx]->formatTexture)
            {
                D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT decode_support = { 0 };
                decode_support.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
                decode_support.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
                decode_support.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
                decode_support.DecodeFormat = processorInput[idx]->formatTexture;
                decode_support.Width = s->width;
                decode_support.Height = s->height;
                decode_support.FrameRate.Denominator = pDec->m_pAVCtx->framerate.den;
                decode_support.FrameRate.Numerator = pDec->m_pAVCtx->framerate.num;
                /*if (fmt->i_frame_rate && fmt->i_frame_rate_base)
                {
                    decode_support.FrameRate =
                        (DXGI_RATIONAL){ .Numerator = fmt->i_frame_rate, .Denominator = fmt->i_frame_rate_base };
                }*/
                hr = pDec->m_pVideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &decode_support, sizeof(decode_support));

                if (SUCCEEDED(hr))
                {
                    if (decode_support.SupportFlags & D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED)
                    {

                        if (processorInput[idx]->formatTexture == DXGI_FORMAT_NV12)
                        {
                            is_supported = true;
                            pDec->render_fmt = processorInput[idx];
                            break;
                        }

                    }

                }
            }
            if (is_supported)
                break;
        }
    }
    unsigned surface_idx;
    pDec->m_SurfaceFormat = pDec->render_fmt->formatTexture;
    CD3DX12_HEAP_PROPERTIES m_textureProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_CUSTOM, 1, 1);
    CD3DX12_RESOURCE_DESC m_textureDesc = CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, s->width, s->height, 1/*pDec->GetBufferCount()*/, 1, pDec->m_SurfaceFormat, 1, 0, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE);
    m_textureProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE;
    m_textureProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L1;
    //to do verify if device handle it
    m_textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
    
    for (surface_idx = 0; surface_idx < pDec->GetBufferCount(); surface_idx++)
    {
        hr = pDec->m_pD3DDevice->CreateCommittedResource(&m_textureProp, D3D12_HEAP_FLAG_NONE,
            &m_textureDesc, D3D12_RESOURCE_STATE_VIDEO_DECODE_READ, NULL,
            IID_ID3D12Resource, (void**)&pDec->m_pTexture[surface_idx]);
        if (FAILED(hr))
            assert(0);
    }
    

    pDec->m_pD3D12VAContext.surfaces.NumTexture2Ds = pDec->GetBufferCount();
    pDec->m_pD3D12VAContext.surfaces.ppTexture2Ds = pDec->ref_table;
    pDec->m_pD3D12VAContext.surfaces.pSubresources = pDec->ref_index;
    pDec->m_pD3D12VAContext.surfaces.ppHeaps = NULL;
    for (surface_idx = 0; surface_idx < pDec->GetBufferCount(); surface_idx++) {
        pDec->ref_table[surface_idx] = pDec->m_pTexture[surface_idx];
        pDec->ref_index[surface_idx] = 0;// surface_idx;
    }
    decoderDesc.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
    decoderDesc.NodeMask = 0;
    decoderDesc.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
    decoderDesc.Configuration.BitstreamEncryption = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;

    ID3D12VideoDecoder* decoder;

    hr = pDec->m_pVideoDevice->CreateVideoDecoder(&decoderDesc, IID_ID3D12VideoDecoder, (void**)&decoder);
    pDec->m_pVideoDecoder = decoder;
    pDec->m_pD3D12VAContext.workaround = 0;
    pDec->m_pD3D12VAContext.report_id = 0;
    pDec->m_pD3D12VAContext.decoder = decoder;
    pDec->m_pVideoDecoderCfg.NodeMask = 0;
    pDec->m_pVideoDecoderCfg.Configuration = decoderDesc.Configuration;
    pDec->m_pVideoDecoderCfg.DecodeWidth = s->width;
    pDec->m_pVideoDecoderCfg.DecodeHeight = s->height;
    pDec->m_pVideoDecoderCfg.Format = pDec->m_SurfaceFormat;
    pDec->m_pVideoDecoderCfg.MaxDecodePictureBufferCount = pDec->GetBufferCount();
    pDec->m_pD3D12VAContext.cfg = &pDec->m_pVideoDecoderCfg;
    pDec->m_pVideoDecoderCfg.FrameRate.Denominator = pDec->m_pAVCtx->framerate.den;
    pDec->m_pVideoDecoderCfg.FrameRate.Numerator = pDec->m_pAVCtx->framerate.num;
    pDec->m_pAVCtx->hwaccel_context = &pDec->m_pD3D12VAContext;
    ref_free_index.clear();
    ref_free_index.resize(0);
    for (int i = 0; i < pDec->GetBufferCount(); i++)
        ref_free_index.push_back(i);
    return S_OK;
}

STDMETHODIMP CDecD3D12::CreateD3D12Decoder()
{
    HRESULT hr = S_OK;
    
    // update surface properties
    m_dwSurfaceWidth = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_width);
    m_dwSurfaceHeight = dxva_align_dimensions(m_pAVCtx->codec_id, m_pAVCtx->coded_height);
    
    m_dwSurfaceCount = GetBufferCount();
    
    return E_FAIL;
}

STDMETHODIMP CDecD3D12::AllocateFramesContext(int width, int height, AVPixelFormat format, int nSurfaces,
                                              AVBufferRef **ppFramesCtx)
{
    ASSERT(m_pAVCtx);
    
    ASSERT(ppFramesCtx);
    av_buffer_unref(ppFramesCtx);
    
    //*ppFramesCtx = av_hwframe_ctx_alloc(m_pDevCtx);
    if (*ppFramesCtx == nullptr)
        return E_OUTOFMEMORY;

    AVHWFramesContext *pFrames = (AVHWFramesContext *)(*ppFramesCtx)->data;
    AVHWDeviceContext *device_ctx = pFrames->device_ctx;
    
    pFrames->format = AV_PIX_FMT_D3D12_VLD;
    pFrames->sw_format = (format == AV_PIX_FMT_YUV420P10) ? AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
    pFrames->width = width;
    pFrames->height = height;
    pFrames->initial_pool_size = nSurfaces;
    
    //DeviceContext->get_decoder_buffer = GetDecodeBuffer;
    //pDeviceContext->set_picparams_h264 = SetPicParamsH264;
    //pDeviceContext->create_d3d12ressource = CreateD3D12Ressource;
    int ret = av_hwframe_ctx_init(*ppFramesCtx);
    
    /* char errbuf[128];
    const char *errbuf_ptr = errbuf;
    int ret2 = av_strerror(ret, errbuf, sizeof(errbuf));
    ret2 = ENOMEM;*/
    if (ret < 0)
    {
        //av_buffer_unref(ppFramesCtx);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CDecD3D12::HandleDXVA2Frame(LAVFrame *pFrame)
{
    
    if (pFrame->flags & LAV_FRAME_FLAG_FLUSH)
    {
        if (m_bReadBackFallback)
        {
            FlushDisplayQueue(TRUE);
        }
        Deliver(pFrame);
        return S_OK;
    }

    ASSERT(pFrame->format == LAVPixFmt_D3D12);

    if (m_bReadBackFallback == false)
    {
        DeliverD3D12Frame(pFrame);
    }
    else
    {
        DeliverD3D12Frame(pFrame);
        
    }

    return S_OK;
}

HRESULT CDecD3D12::DeliverD3D12Frame(LAVFrame *pFrame)
{
    if (m_bReadBackFallback)
    {
        if (m_bDirect)
            DeliverD3D12ReadbackDirect(pFrame);
        else
            DeliverD3D12Readback(pFrame);
    }
    else
    {
        AVFrame* pAVFrame = (AVFrame*)pFrame->priv_data;
        pFrame->data[0] = pAVFrame->data[3];
        pFrame->data[1] = pFrame->data[2] = pFrame->data[3] = nullptr;

        GetPixelFormat(&pFrame->format, &pFrame->bpp);

        Deliver(pFrame);
    }

    return S_OK;
}

HRESULT CDecD3D12::DeliverD3D12Readback(LAVFrame *pFrame)
{
    
    return Deliver(pFrame);
}

struct D3D12DirectPrivate
{
    //AVBufferRef *pDeviceContex;
    d3d12_footprint_t pStagingLayout;
    ID3D12Resource *pStagingTexture;
    int frame_index;
    CDecD3D12* pDec;
};

static bool d3d12_direct_lock(LAVFrame *pFrame, LAVDirectBuffer *pBuffer)
{
    D3D12DirectPrivate* c = (D3D12DirectPrivate*)pFrame->priv_data;
    
    D3D12_RESOURCE_DESC desc;
    

    ASSERT(pFrame && pBuffer);

    // lock the device context
    //pDeviceContext->lock(pDeviceContext->lock_ctx);

    desc = c->pStagingTexture->GetDesc();

    // map
    HRESULT hr = S_OK;// pDeviceContext->device_context->Map(c->pStagingTexture, 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr))
    {
        //pDeviceContext->unlock(pDeviceContext->lock_ctx);
        return false;
    }
    void* pData;
    D3D12_RANGE readRange;
    readRange.Begin = 0;
    readRange.End = c->pStagingLayout.RequiredSize;
    
    hr = c->pStagingTexture->Map(0, &readRange,&pData);
    //D3D12_BOX pSrcBox;
    //675840 the data stop at that
    //hr = c->pStagingTexture->ReadFromSubresource(&pData, c->pStagingLayout.layoutplane[0].Footprint.RowPitch, 1,(UINT)0,nullptr);
    pBuffer->data[0] = (BYTE*)pData;
    pBuffer->data[1] = pBuffer->data[0] + c->pStagingLayout.layoutplane[0].Footprint.Height * c->pStagingLayout.layoutplane[0].Footprint.RowPitch;// map.RowPitch;

    pBuffer->stride[0] = c->pStagingLayout.layoutplane[0].Footprint.RowPitch;
    pBuffer->stride[1] = c->pStagingLayout.layoutplane[0].Footprint.RowPitch;

    //c->pStagingTexture->Unmap(0, nullptr);
    return true;
}

static void d3d12_direct_unlock(LAVFrame *pFrame)
{
    D3D12DirectPrivate* c = (D3D12DirectPrivate*)pFrame->priv_data;
    c->pStagingTexture->Unmap(0, nullptr);
}

static void d3d12_direct_free(LAVFrame *pFrame)
{
    D3D12DirectPrivate *c = (D3D12DirectPrivate *)pFrame->priv_data;
    
    c->pStagingTexture->Release();
    c->pStagingLayout = c->pStagingLayout;
    c->pDec->free_buffer(c->frame_index);
    delete c;
}
void CDecD3D12::free_buffer(int idx)
{
    ref_free_index.push_back(idx);
    
}

HRESULT CDecD3D12::UpdateStaging()
{
    
    HRESULT hr = S_OK;
    if (!m_pD3DCommands)
    {
        m_pD3DCommands = new CD3D12Commands(m_pD3DDevice);
        m_pStagingDesc = {};
        m_pStagingProp = {};
        UINT64 sss;
        D3D12_RESOURCE_DESC indesc = ref_table[0]->GetDesc();
        //nv12
        //DXGI_FORMAT_R8_UNORM first plane
        //DXGI_FORMAT_R8G8_UNORM second plane
        m_pD3DDevice->GetCopyableFootprints(&indesc,
            0, 2, 0, m_pStagingLayout.layoutplane, m_pStagingLayout.rows_plane, m_pStagingLayout.pitch_plane, &m_pStagingLayout.RequiredSize);


        m_pStagingProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        m_pStagingDesc = CD3DX12_RESOURCE_DESC::Buffer(m_pStagingLayout.RequiredSize);


        hr = m_pD3DDevice->CreateCommittedResource(&m_pStagingProp, D3D12_HEAP_FLAG_NONE,
            &m_pStagingDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL,//D3D12_RESOURCE_STATE_GENERIC_READ
            IID_ID3D12Resource, (void**)&m_pStagingTexture);

        if (FAILED(hr))
            assert(0);
    }
    return hr;

}



HRESULT CDecD3D12::DeliverD3D12ReadbackDirect(LAVFrame *pFrame)
{
    AVFrame* srcframe = (AVFrame*)pFrame->priv_data;
    UINT outputindex;

    outputindex = (UINT)srcframe->data[3];
    ID3D12Resource* out = m_pTexture[outputindex];
    D3D12_RESOURCE_DESC desc = m_pTexture[outputindex]->GetDesc();
    UINT outsub = m_pD3D12VAContext.surfaces.pSubresources[outputindex];
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts[2] = {};
    UINT numrows[2] = {};
    UINT64 rowsizes[2] = {};
    UINT64 totalbytes = 0;
    D3D12_FEATURE_DATA_FORMAT_INFO info;
    UpdateStaging();
    
    info.Format = DXGI_FORMAT_NV12;
    m_pD3DDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info));
    
    //D3D12_RESOURCE_DESC desc = m_pStagingTexture->GetDesc();
    HRESULT hr;
    ID3D12GraphicsCommandList *cmd = m_pD3DCommands->GetCommandBuffer();
    
    m_pD3DCommands->Reset(cmd);
    
    
    D3D12_TEXTURE_COPY_LOCATION dst;
    D3D12_TEXTURE_COPY_LOCATION src;

    D3D12_BOX box;
    box = CD3DX12_BOX(0, 0, m_pAVCtx->width, m_pAVCtx->height);

    
    for (int i = 0; i < 2; i++) {
        dst = CD3DX12_TEXTURE_COPY_LOCATION(m_pStagingTexture, m_pStagingLayout.layoutplane[i]);

        src = CD3DX12_TEXTURE_COPY_LOCATION(out, i);
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        
    }
    uint8_t* pdata;
    D3D12_RANGE range;
    range.Begin = 0;
    range.End = m_pStagingLayout.RequiredSize;
    
    
    hr = cmd->Close();
    m_pD3DCommands->Submit({ cmd });
    m_pD3DCommands->GPUSync();

    D3D12DirectPrivate* c = new D3D12DirectPrivate;
    //c->pDeviceContex = av_buffer_ref(m_pDevCtx);
    
    c->pStagingTexture = m_pStagingTexture;
    c->pStagingLayout = m_pStagingLayout;
    c->frame_index = (int)outputindex;
    c->pDec = this;
    m_pStagingTexture->AddRef();
    pFrame->priv_data = c;
    pFrame->destruct = d3d12_direct_free;

    GetPixelFormat(&pFrame->format, &pFrame->bpp);

    pFrame->direct = true;
    pFrame->direct_lock = d3d12_direct_lock;
    pFrame->direct_unlock = d3d12_direct_unlock;

    return Deliver(pFrame);
}

STDMETHODIMP CDecD3D12::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
    //*pPix = LAVPixFmt_D3D12;
    //*pBpp = 16;
    //return S_OK;
    // Output is always NV12 or P010
    if (pPix)
        *pPix = LAVPixFmt_NV12;/*m_bReadBackFallback == false
                    ? LAVPixFmt_D3D12
                   : ((m_SurfaceFormat == DXGI_FORMAT_NV12) ? LAVPixFmt_P016 : LAVPixFmt_NV12);*/

    if (pBpp)
        *pBpp = (m_SurfaceFormat == DXGI_FORMAT_P016) ? 16 : (m_SurfaceFormat == DXGI_FORMAT_P010 ? 10 : 8);

    return S_OK;
}

STDMETHODIMP_(DWORD) CDecD3D12::GetHWAccelNumDevices()
{
    DWORD nDevices = 0;
    UINT i = 0;
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;

    HRESULT hr = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
        goto fail;

    DXGI_ADAPTER_DESC desc;
    while (SUCCEEDED(pDXGIFactory->EnumAdapters(i, &pDXGIAdapter)))
    {
        pDXGIAdapter->GetDesc(&desc);
        SafeRelease(&pDXGIAdapter);

        // stop when we hit the MS software device
        if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
            break;

        i++;
    }

    nDevices = i;

fail:
    SafeRelease(&pDXGIFactory);
    return nDevices;
}

STDMETHODIMP CDecD3D12::GetHWAccelDeviceInfo(DWORD dwIndex, BSTR *pstrDeviceName, DWORD *dwDeviceIdentifier)
{
    IDXGIAdapter *pDXGIAdapter = nullptr;
    IDXGIFactory1 *pDXGIFactory = nullptr;
    
    HRESULT hr = CreateDXGIFactory1(IID_IDXGIFactory1, (void **)&pDXGIFactory);
    if (FAILED(hr))
        goto fail;

    hr = pDXGIFactory->EnumAdapters(dwIndex, &pDXGIAdapter);
    if (FAILED(hr))
        goto fail;

    DXGI_ADAPTER_DESC desc;
    pDXGIAdapter->GetDesc(&desc);

    // stop when we hit the MS software device
    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
    {
        hr = E_INVALIDARG;
        goto fail;
    }

    if (pstrDeviceName)
        *pstrDeviceName = SysAllocString(desc.Description);

    if (dwDeviceIdentifier)
        *dwDeviceIdentifier = desc.DeviceId;

fail:
    SafeRelease(&pDXGIFactory);
    SafeRelease(&pDXGIAdapter);
    return hr;
}

STDMETHODIMP CDecD3D12::GetHWAccelActiveDevice(BSTR *pstrDeviceName)
{
    CheckPointer(pstrDeviceName, E_POINTER);

    if (m_AdapterDesc.Description[0] == 0)
        return E_UNEXPECTED;

    *pstrDeviceName = SysAllocString(m_AdapterDesc.Description);
    return S_OK;
}
