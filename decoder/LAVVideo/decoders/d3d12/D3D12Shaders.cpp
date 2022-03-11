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
#include "D3D12Shaders.h"
#include <vector>

static const char globPixelShaderDefault[] = "\
cbuffer PS_CONSTANT_BUFFER : register(b0)\n\
{\n\
    float4x3 Colorspace;\n\
    float Opacity;\n\
    float LuminanceScale;\n\
    float2 Boundary;\n\
};\n\
Texture2D shaderTexture[TEXTURE_RESOURCES];\n\
SamplerState normalSampler : register(s0);\n\
SamplerState borderSampler : register(s1);\n\
\n\
struct PS_INPUT\n\
{\n\
    float4 Position   : SV_POSITION;\n\
    float2 uv         : TEXCOORD;\n\
};\n\
\n\
#define TONE_MAP_HABLE       1\n\
\n\
#define SRC_TRANSFER_709     1\n\
#define SRC_TRANSFER_PQ      2\n\
#define SRC_TRANSFER_HLG     3\n\
#define SRC_TRANSFER_SRGB    4\n\
#define SRC_TRANSFER_470BG   5\n\
\n\
#define DST_TRANSFER_SRGB    1\n\
#define DST_TRANSFER_PQ      2\n\
\n\
#define FULL_RANGE           1\n\
#define STUDIO_RANGE         2\n\
\n\
#define SAMPLE_NV12_TO_YUVA          1\n\
#define SAMPLE_YUY2_TO_YUVA          2\n\
#define SAMPLE_Y210_TO_YUVA          3\n\
#define SAMPLE_Y410_TO_YUVA          4\n\
#define SAMPLE_AYUV_TO_YUVA          5\n\
#define SAMPLE_RGBA_TO_RGBA          6\n\
#define SAMPLE_TRIPLANAR_TO_YUVA     7\n\
#define SAMPLE_TRIPLANAR10_TO_YUVA   8\n\
#define SAMPLE_PLANAR_YUVA_TO_YUVA   9\n\
\n\
#define SAMPLE_NV12_TO_NV_Y         10\n\
#define SAMPLE_NV12_TO_NV_UV        11\n\
#define SAMPLE_RGBA_TO_NV_R         12\n\
#define SAMPLE_RGBA_TO_NV_GB        13\n\
#define SAMPLE_PLANAR_YUVA_TO_NV_Y  14\n\
#define SAMPLE_PLANAR_YUVA_TO_NV_UV 15\n\
\n\
#if (TONE_MAPPING==TONE_MAP_HABLE)\n\
/* see http://filmicworlds.com/blog/filmic-tonemapping-operators/ */\n\
inline float3 hable(float3 x) {\n\
    const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;\n\
    return ((x * (A*x + (C*B))+(D*E))/(x * (A*x + B) + (D*F))) - E/F;\n\
}\n\
#endif\n\
\n\
#if (SRC_TO_LINEAR==SRC_TRANSFER_HLG)\n\
/* https://en.wikipedia.org/wiki/Hybrid_Log-Gamma#Technical_details */\n\
inline float inverse_HLG(float x){\n\
    const float B67_a = 0.17883277;\n\
    const float B67_b = 0.28466892;\n\
    const float B67_c = 0.55991073;\n\
    const float B67_inv_r2 = 4.0; /* 1/0.5² */\n\
    if (x <= 0.5)\n\
        x = x * x * B67_inv_r2;\n\
    else\n\
        x = exp((x - B67_c) / B67_a) + B67_b;\n\
    return x;\n\
}\n\
#endif\n\
\n\
inline float3 sourceToLinear(float3 rgb) {\n\
#if (SRC_TO_LINEAR==SRC_TRANSFER_PQ)\n\
    const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
    const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
    const float ST2084_c1 = 3424.0 / 4096.0;\n\
    const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
    const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n\
    rgb = pow(max(rgb, 0), 1.0/ST2084_m2);\n\
    rgb = max(rgb - ST2084_c1, 0.0) / (ST2084_c2 - ST2084_c3 * rgb);\n\
    rgb = pow(rgb, 1.0/ST2084_m1);\n\
    return rgb * 10000;\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_HLG)\n\
    /* TODO: in one call */\n\
    rgb.r = inverse_HLG(rgb.r);\n\
    rgb.g = inverse_HLG(rgb.g);\n\
    rgb.b = inverse_HLG(rgb.b);\n\
    float3 ootf_2020 = float3(0.2627, 0.6780, 0.0593);\n\
    float ootf_ys = 1000 * dot(ootf_2020, rgb);\n\
    return rgb * pow(ootf_ys, 0.200);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_709)\n\
    return pow(rgb, 1.0 / 0.45);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_SRGB)\n\
    return pow(rgb, 2.2);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_470BG)\n\
    return pow(rgb, 2.8);\n\
#else\n\
    return rgb;\n\
#endif\n\
}\n\
\n\
inline float3 linearToDisplay(float3 rgb) {\n\
#if (LINEAR_TO_DST==DST_TRANSFER_SRGB)\n\
    return pow(rgb, 1.0 / 2.2);\n\
#elif (LINEAR_TO_DST==DST_TRANSFER_PQ)\n\
    const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
    const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
    const float ST2084_c1 = 3424.0 / 4096.0;\n\
    const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
    const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n\
    rgb = pow(rgb / 10000, ST2084_m1);\n\
    rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);\n\
    return pow(rgb, ST2084_m2);\n\
#else\n\
    return rgb;\n\
#endif\n\
}\n\
\n\
inline float3 toneMapping(float3 rgb) {\n\
    rgb = rgb * LuminanceScale;\n\
#if (TONE_MAPPING==TONE_MAP_HABLE)\n\
    const float3 HABLE_DIV = hable(11.2);\n\
    rgb = hable(rgb) / HABLE_DIV;\n\
#endif\n\
    return rgb;\n\
}\n\
\n\
inline float3 adjustRange(float3 rgb) {\n\
#if (SRC_RANGE!=DST_RANGE)\n\
    return clamp((rgb + BLACK_LEVEL_SHIFT) * RANGE_FACTOR, MIN_BLACK_VALUE, MAX_WHITE_VALUE);\n\
#else\n\
    return rgb;\n\
#endif\n\
}\n\
\n\
inline float4 sampleTexture(SamplerState samplerState, float2 coords) {\n\
    float4 sample;\n\
    /* sampling routine in sample */\n\
#if (SAMPLE_TEXTURES==SAMPLE_NV12_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.yz = shaderTexture[1].Sample(samplerState, coords).xy;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_YUY2_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_Y210_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).r;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).g;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_Y410_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).g;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).r;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).b;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_AYUV_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).z;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_RGBA)\n\
    sample = shaderTexture[0].Sample(samplerState, coords);\n\
#elif (SAMPLE_TEXTURES==SAMPLE_TRIPLANAR_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_TRIPLANAR10_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample = sample * 64;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.a  = shaderTexture[3].Sample(samplerState, coords).x;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_NV12_TO_NV_Y)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y = 0.0;\n\
    sample.z = 0.0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_NV12_TO_NV_UV)\n\
    // TODO should be shaderTexture[0] ?\n\
    sample.xy  = shaderTexture[1].Sample(samplerState, coords).xy;\n\
    sample.z = 0.0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_NV_R)\n\
    sample = shaderTexture[0].Sample(samplerState, coords);\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_NV_GB)\n\
    sample.x = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.y = shaderTexture[0].Sample(samplerState, coords).z;\n\
    sample.z = 0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_NV_Y)\n\
    sample.x = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y = 0.0;\n\
    sample.z = 0.0;\n\
    sample.a = shaderTexture[3].Sample(samplerState, coords).x;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_NV_UV)\n\
    sample.x = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.y = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.z = 0.0;\n\
    sample.a = shaderTexture[3].Sample(samplerState, coords).x;\n\
#endif\n\
    return sample;\n\
}\n\
\n\
float4 main( PS_INPUT In ) : SV_TARGET\n\
{\n\
    float4 sample;\n\
    \n\
    if (In.uv.x > Boundary.x || In.uv.y > Boundary.y) \n\
        sample = sampleTexture( borderSampler, In.uv );\n\
    else\n\
        sample = sampleTexture( normalSampler, In.uv );\n\
    float3 rgb = max(mul(sample, Colorspace),0);\n\
    rgb = sourceToLinear(rgb);\n\
    rgb = toneMapping(rgb);\n\
    rgb = linearToDisplay(rgb);\n\
    rgb = adjustRange(rgb);\n\
    return float4(rgb, saturate(sample.a * Opacity));\n\
}\n\
";

static const char globVertexShader[] = "\n\
#if HAS_PROJECTION\n\
cbuffer VS_PROJECTION_CONST : register(b0)\n\
{\n\
    float4x4 View;\n\
    float4x4 Zoom;\n\
    float4x4 Projection;\n\
};\n\
#endif\n\
struct d3d_vertex_t\n\
{\n\
    float3 Position   : POSITION;\n\
    float2 uv         : TEXCOORD;\n\
};\n\
\n\
struct PS_INPUT\n\
{\n\
    float4 Position   : SV_POSITION;\n\
    float2 uv         : TEXCOORD;\n\
};\n\
\n\
PS_INPUT main( d3d_vertex_t In )\n\
{\n\
    PS_INPUT Output;\n\
    float4 pos = float4(In.Position, 1);\n\
#if HAS_PROJECTION\n\
    pos = mul(View, pos);\n\
    pos = mul(Zoom, pos);\n\
    pos = mul(Projection, pos);\n\
#endif\n\
    Output.Position = pos;\n\
    Output.uv = In.uv;\n\
    return Output;\n\
}\n\
";

static void ReleaseID3D10Blob(d3d_shader_blob* blob)
{
    ((ID3D10Blob*)blob->opaque)->Release();
}

static void ID3D10BlobtoBlob(ID3D10Blob* d3dblob, d3d_shader_blob* blob)
{
    blob->opaque = d3dblob;
    blob->pf_release = ReleaseID3D10Blob;
    blob->buf_size = d3dblob->GetBufferSize();
    blob->buffer = d3dblob->GetBufferPointer();
}

static HRESULT CompileShader(const d3d_shader_compiler_t* compiler,
    D3D_FEATURE_LEVEL feature_level,
    const char* psz_shader, bool pixelShader,
    const D3D_SHADER_MACRO* defines,
    d3d_shader_blob* blob)
{
    ID3D10Blob* pShaderBlob = NULL, * pErrBlob = NULL;

    UINT compileFlags = 0;
# if !defined(NDEBUG)
    if (IsDebuggerPresent())
        compileFlags += 1 << 0;//D3DCOMPILE_DEBUG;
#endif
    HRESULT hr;
    {
        const char* target;
        if (pixelShader)
        {
            if (feature_level >= D3D_FEATURE_LEVEL_12_0)
                target = "ps_5_0";
            else if (!!(feature_level >= D3D_FEATURE_LEVEL_10_0))
                target = "ps_4_0";
            else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
                target = "ps_4_0_level_9_3";
            else
                target = "ps_4_0_level_9_1";
        }
        else
        {
            if (feature_level >= D3D_FEATURE_LEVEL_12_0)
                target = "vs_5_0";
            else if (!!(feature_level >= D3D_FEATURE_LEVEL_10_0))
                target = "vs_4_0";
            else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
                target = "vs_4_0_level_9_3";
            else
                target = "vs_4_0_level_9_1";
        }

        hr = compiler->OurD3DCompile(psz_shader, strlen(psz_shader),
            NULL, defines, NULL, "main", target,
            compileFlags, 0, &pShaderBlob, &pErrBlob);
    }

    if (FAILED(hr) || pErrBlob) {
        const char* err = pErrBlob ? (char*)pErrBlob->GetBufferPointer() : NULL;
        DbgLog((LOG_TRACE, 10, L"invalid %s Shader (hr=0x%lX): %s", pixelShader ? "Pixel" : "Vertex", hr, err));
        if (pErrBlob)
            pErrBlob->Release();
        if (FAILED(hr))
            return hr;
    }
    if (!pShaderBlob)
        return E_INVALIDARG;
    ID3D10BlobtoBlob(pShaderBlob, blob);
    return S_OK;
}


CD3D12Shaders::CD3D12Shaders()
{
}

CD3D12Shaders::~CD3D12Shaders()
{
}



HRESULT CD3D12Shaders::HeapInit(ID3D12Device* d3d_dev, d3d12_heap_t* heap,
    D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags,
    UINT NumDescriptors)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
    ZeroMemory(&heapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
    heapDesc.NumDescriptors = NumDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = flags;

    HRESULT hr;
    hr = d3d_dev->CreateDescriptorHeap(&heapDesc, IID_ID3D12DescriptorHeap, (void**)&heap->heap);
    if (FAILED(hr))
        return hr;

    heap->m_descriptorSize = d3d_dev->GetDescriptorHandleIncrementSize(type);
    return S_OK;
}

void CD3D12Shaders::HeapRelease(d3d12_heap_t* heap)
{
    if (heap->heap)
    {
        heap->heap->Release();
        heap->heap = NULL;
    }
}


void CD3D12Shaders::CommandListRelease(d3d12_commands_t* cmd)
{
    if (cmd->m_commandAllocator)
    {
        cmd->m_commandAllocator->Release();
        cmd->m_commandAllocator = NULL;
    }
    if (cmd->m_commandList)
    {
        cmd->m_commandList->Release();
        cmd->m_commandList = NULL;
    }
}

static HRESULT CompilePixelShaderBlob(const d3d_shader_compiler_t* compiler,
    D3D_FEATURE_LEVEL feature_level,
    const char* psz_sampler,
    const char* psz_shader_resource_views,
    const char* psz_src_to_linear,
    const char* psz_linear_to_display,
    const char* psz_tone_mapping,
    const char* psz_src_range, const char* psz_dst_range,
    const char* psz_black_level,
    const char* psz_range_factor,
    const char* psz_min_black,
    const char* psz_max_white,
    d3d_shader_blob* pPSBlob)
{
    D3D_SHADER_MACRO defines[] = {
         { "TEXTURE_RESOURCES", psz_shader_resource_views },
         { "TONE_MAPPING",      psz_tone_mapping },
         { "SRC_TO_LINEAR",     psz_src_to_linear },
         { "LINEAR_TO_DST",     psz_linear_to_display },
         { "SAMPLE_TEXTURES",   psz_sampler },
         { "SRC_RANGE",         psz_src_range },
         { "DST_RANGE",         psz_dst_range },
         { "BLACK_LEVEL_SHIFT", psz_black_level },
         { "RANGE_FACTOR",      psz_range_factor },
         { "MIN_BLACK_VALUE",   psz_min_black },
         { "MAX_WHITE_VALUE",   psz_max_white },
         { NULL, NULL },
    };
#ifndef NDEBUG
    size_t param = 0;
    while (defines[param].Name)
    {
        DbgLog((LOG_TRACE, 10, L"%s = %s", defines[param].Name, defines[param].Definition));
        param++;
    }
#endif

    return CompileShader(compiler, feature_level, globPixelShaderDefault, true,
        defines, pPSBlob);
}

HRESULT CompilePixelShader(const d3d_shader_compiler_t* compiler,
    D3D_FEATURE_LEVEL feature_level,
    const display_info_t* display,
    video_transfer_func_t transfer,
    bool src_full_range,
    const d3d_format_t* dxgi_fmt,
    d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET],
    size_t shader_views[DXGI_MAX_RENDER_TARGET])
{
    static const char* DEFAULT_NOOP = "0";
    const char* psz_sampler[DXGI_MAX_RENDER_TARGET] = { NULL, NULL };
    const char* psz_shader_resource_views[DXGI_MAX_RENDER_TARGET] = { NULL, NULL };
    const char* psz_src_to_linear = DEFAULT_NOOP;
    const char* psz_linear_to_display = DEFAULT_NOOP;
    const char* psz_tone_mapping = DEFAULT_NOOP;
    const char* psz_src_range, * psz_dst_range;
    shader_views[1] = 0;

    if (display->pixelFormat->formatTexture == DXGI_FORMAT_NV12 ||
        display->pixelFormat->formatTexture == DXGI_FORMAT_P010)
    {
        /* we need 2 shaders, one for the Y target, one for the UV target */
        switch (dxgi_fmt->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] = "SAMPLE_NV12_TO_NV_Y";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            psz_sampler[1] = "SAMPLE_NV12_TO_NV_UV";
            psz_shader_resource_views[1] = "2"; shader_views[1] = 2; // TODO should be 1 ?
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            /* Y */
            psz_sampler[0] = "SAMPLE_RGBA_TO_NV_R";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            /* UV */
            psz_sampler[1] = "SAMPLE_RGBA_TO_NV_GB";
            psz_shader_resource_views[1] = "1"; shader_views[1] = 1;
            break;
        case DXGI_FORMAT_UNKNOWN:
            switch (dxgi_fmt->fourcc)
            {
#if 0
            case VLC_CODEC_YUVA:
                /* Y */
                psz_sampler[0] = "SAMPLE_PLANAR_YUVA_TO_NV_Y";
                psz_shader_resource_views[0] = "4"; shader_views[0] = 4;
                /* UV */
                psz_sampler[1] = "SAMPLE_PLANAR_YUVA_TO_NV_UV";
                psz_shader_resource_views[1] = "4"; shader_views[1] = 4;
                break;
#endif 
            default:
                assert(0);
            }
            break;
        default:
            assert(0);;
        }
    }
    else
    {
        switch (dxgi_fmt->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] = "SAMPLE_NV12_TO_YUVA";
            psz_shader_resource_views[0] = "2"; shader_views[0] = 2;
            break;
        case DXGI_FORMAT_YUY2:
            psz_sampler[0] = "SAMPLE_YUY2_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_Y210:
            psz_sampler[0] = "SAMPLE_Y210_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_Y410:
            psz_sampler[0] = "SAMPLE_Y410_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_AYUV:
            psz_sampler[0] = "SAMPLE_AYUV_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            psz_sampler[0] = "SAMPLE_RGBA_TO_RGBA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_UNKNOWN:

            switch (dxgi_fmt->fourcc)
            {
#if 0
            case VLC_CODEC_I420_10L:
                psz_sampler[0] = "SAMPLE_TRIPLANAR10_TO_YUVA";
                psz_shader_resource_views[0] = "3"; shader_views[0] = 3;
                break;
            case VLC_CODEC_I444_16L:
            case VLC_CODEC_I420:
                psz_sampler[0] = "SAMPLE_TRIPLANAR_TO_YUVA";
                psz_shader_resource_views[0] = "3"; shader_views[0] = 3;
                break;
            case VLC_CODEC_YUVA:
                psz_sampler[0] = "SAMPLE_PLANAR_YUVA_TO_YUVA";
                psz_shader_resource_views[0] = "4"; shader_views[0] = 4;
                break;
#endif
            default:
                assert(0);;
            }
            break;
        default:
            assert(0);;
        }
    }
    video_transfer_func_t src_transfer;

    if (transfer != display->transfer)
    {
        /* we need to go in linear mode */
        switch (transfer)
        {
        case TRANSFER_FUNC_SMPTE_ST2084:
            /* ST2084 to Linear */
            psz_src_to_linear = "SRC_TRANSFER_PQ";
            src_transfer = TRANSFER_FUNC_LINEAR;
            break;
        case TRANSFER_FUNC_HLG:
            psz_src_to_linear = "SRC_TRANSFER_HLG";
            src_transfer = TRANSFER_FUNC_LINEAR;
            break;
        case TRANSFER_FUNC_BT709:
            psz_src_to_linear = "SRC_TRANSFER_709";
            src_transfer = TRANSFER_FUNC_LINEAR;
            break;
        case TRANSFER_FUNC_BT470_M:
        case TRANSFER_FUNC_SRGB:
            psz_src_to_linear = "SRC_TRANSFER_SRGB";
            src_transfer = TRANSFER_FUNC_LINEAR;
            break;
        case TRANSFER_FUNC_BT470_BG:
            psz_src_to_linear = "SRC_TRANSFER_470BG";
            src_transfer = TRANSFER_FUNC_LINEAR;
            break;
        default:
            DbgLog((LOG_TRACE, 10, L"unhandled source transfer"));
            src_transfer = transfer;
            break;
        }

        switch (display->transfer)
        {
        case TRANSFER_FUNC_SRGB:
            if (src_transfer == TRANSFER_FUNC_LINEAR)
            {
                /* Linear to sRGB */
                psz_linear_to_display = "DST_TRANSFER_SRGB";

                if (transfer == TRANSFER_FUNC_SMPTE_ST2084 || transfer == TRANSFER_FUNC_HLG)
                {
                    /* HDR tone mapping */
                    psz_tone_mapping = "TONE_MAP_HABLE";
                }
            }
            else
                DbgLog((LOG_TRACE, 10, L"don't know how to transfer from %d to sRGB", src_transfer));
            break;

        case TRANSFER_FUNC_SMPTE_ST2084:
            if (src_transfer == TRANSFER_FUNC_LINEAR)
            {
                /* Linear to ST2084 */
                psz_linear_to_display = "DST_TRANSFER_PQ";
            }
            else
                DbgLog((LOG_TRACE, 10, L"don't know how to transfer from %d to SMPTE ST 2084", src_transfer));
            break;
        default:
            DbgLog((LOG_TRACE, 10, L"don't know how to transfer from %d to %d", src_transfer, display->transfer));
            break;
        }
    }

    bool dst_full_range = display->b_full_range;
    if (!DxgiIsRGBFormat(dxgi_fmt) && DxgiIsRGBFormat(display->pixelFormat))
    {
        /* the YUV->RGB conversion already output full range */
        if (!src_full_range)
            src_full_range = true; // account for this conversion
        else
        {
            if (!dst_full_range)
                DbgLog((LOG_TRACE, 10, L"unsupported display full range YUV on studio range RGB"));
            dst_full_range = false; // force a conversion down
        }
    }

    int range_adjust = 0;
    psz_src_range = src_full_range ? "FULL_RANGE" : "STUDIO_RANGE";
    psz_dst_range = display->b_full_range ? "FULL_RANGE" : "STUDIO_RANGE";
    if (dst_full_range != src_full_range) {
        if (!src_full_range)
            range_adjust = 1; /* raise the source to full range */
        else
            range_adjust = -1; /* lower the source to studio range */
    }

    FLOAT black_level = 0;
    FLOAT range_factor = 1.0f;
    FLOAT itu_black_level = 0.f;
    FLOAT itu_white_level = 1.f;
    if (range_adjust != 0)
    {
        FLOAT itu_range_factor;
        switch (dxgi_fmt->bitsPerChannel)
        {
        case 8:
            /* Rec. ITU-R BT.709-6 §4.6 */
            itu_black_level = 16.f / 255.f;
            itu_white_level = 235.f / 255.f;
            itu_range_factor = (float)(235 - 16) / 255.f;
            break;
        case 10:
            /* Rec. ITU-R BT.709-6 §4.6 */
            itu_black_level = 64.f / 1023.f;
            itu_white_level = 940.f / 1023.f;
            itu_range_factor = (float)(940 - 64) / 1023.f;
            break;
        case 12:
            /* Rec. ITU-R BT.2020-2 Table 5 */
            itu_black_level = 256.f / 4095.f;
            itu_white_level = 3760.f / 4095.f;
            itu_range_factor = (float)(3760 - 256) / 4095.f;
            break;
        default:
            /* unknown bitdepth, use approximation for infinite bit depth */
            itu_black_level = 16.f / 256.f;
            itu_white_level = 235.f / 256.f;
            itu_range_factor = (float)(235 - 16) / 256.f;
            break;
        }

        if (range_adjust > 0)
        {
            /* expand the range from studio to full range */
            black_level -= itu_black_level;
            range_factor /= itu_range_factor;
        }
        else
        {
            /* shrink the range to studio range */
            black_level += itu_black_level;
            range_factor *= itu_range_factor;
        }
    }
    char psz_black_level[20];
    char psz_range_factor[20];
    char psz_min_black[20];
    char psz_max_white[20];
    snprintf(psz_black_level, ARRAY_SIZE(psz_black_level), "%f", black_level);
    snprintf(psz_range_factor, ARRAY_SIZE(psz_range_factor), "%f", range_factor);
    snprintf(psz_min_black, ARRAY_SIZE(psz_min_black), "%f", itu_black_level);
    snprintf(psz_max_white, ARRAY_SIZE(psz_max_white), "%f", itu_white_level);

    HRESULT hr;
    hr = CompilePixelShaderBlob(compiler, feature_level,
        psz_sampler[0], psz_shader_resource_views[0],
        psz_src_to_linear,
        psz_linear_to_display,
        psz_tone_mapping,
        psz_src_range, psz_dst_range,
        psz_black_level, psz_range_factor, psz_min_black, psz_max_white,
        &pPSBlob[0]);
    if (SUCCEEDED(hr) && psz_sampler[1])
    {
        hr = CompilePixelShaderBlob(compiler, feature_level,
            psz_sampler[1], psz_shader_resource_views[1],
            psz_src_to_linear,
            psz_linear_to_display,
            psz_tone_mapping,
            psz_src_range, psz_dst_range,
            psz_black_level, psz_range_factor, psz_min_black, psz_max_white,
            &pPSBlob[1]);
        if (FAILED(hr))
            D3D_ShaderBlobRelease(&pPSBlob[0]);
    }

    return hr;
}

HRESULT CD3D12Shaders::CommandListCreateGraphic(ID3D12Device* d3d_dev, d3d12_commands_t* cmd)
{
    HRESULT hr;
    hr = d3d_dev->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_ID3D12CommandAllocator, (void**)&cmd->m_commandAllocator);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"failed to create quad command allocator. (hr=0x%lX)", hr));
        goto error;
    }
    D3D12_ObjectSetName(cmd->m_commandAllocator, L"quad Command Allocator");

    hr = d3d_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        cmd->m_commandAllocator, NULL/*pipelines[i].m_pipelineState*/,
        IID_ID3D12GraphicsCommandList, (void**)&cmd->m_commandList);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"failed to create command list. (hr=0x%lX)", hr));
        goto error;
    }
    D3D12_ObjectSetName(cmd->m_commandList, L"quad Command List");

    // a command list is recording commands by default
    hr = cmd->m_commandList->Close();
    if (!!(FAILED(hr)))
    {
        DbgLog((LOG_TRACE, 10, L"failed to close command list. (hr=0x%lX)", hr));
        goto error;
    }
    return hr;
error:
    return hr;
}

HRESULT CD3D12Shaders::CompileVertexShader(const d3d_shader_compiler_t* compiler,
    D3D_FEATURE_LEVEL feature_level, bool flat,
    d3d_shader_blob* blob)
{
    D3D_SHADER_MACRO defines[] = {
         { "HAS_PROJECTION", flat ? "0" : "1" },
         { NULL, NULL },
    };
    return CompileShader(compiler, feature_level, globVertexShader, false, defines, blob);
}

HRESULT CD3D12Shaders::CompilePipelineState(const d3d_shader_compiler_t *compiler,
    ID3D12Device* d3d_dev,
    const d3d_format_t* textureFormat,
    const display_info_t* display, bool sharp,
    video_transfer_func_t transfer,
    bool src_full_range,
    video_projection_mode_t projection_mode,
    d3d12_commands_t commands[DXGI_MAX_RENDER_TARGET],
    d3d12_pipeline_t pipelines[DXGI_MAX_RENDER_TARGET],
    size_t shader_views[DXGI_MAX_RENDER_TARGET])
{
    HRESULT hr;
    d3d_shader_blob pVSBlob;
    ZeroMemory(&pVSBlob,sizeof(d3d_shader_blob));
    d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET];
    ZeroMemory(&pPSBlob, sizeof(d3d_shader_blob[DXGI_MAX_RENDER_TARGET]));
    hr = CompilePixelShader(compiler, D3D_FEATURE_LEVEL_12_0,
        display, transfer,
        src_full_range, textureFormat, pPSBlob, shader_views);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"Failed to create the pixel  shader."));
        return hr;
    }

    hr = CompileVertexShader(compiler, D3D_FEATURE_LEVEL_12_0,
        projection_mode == PROJECTION_MODE_RECTANGULAR,
        &pVSBlob);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"Failed to create the vertex shader."));
        goto error;
    }

    for (size_t i = 0; i < DXGI_MAX_RENDER_TARGET; i++)
    {
        if (!pPSBlob[i].buffer)
        {
            ZeroMemory(&commands[i], sizeof(d3d12_commands_t));
            ZeroMemory(&pipelines[i], sizeof(d3d12_pipeline_t));
            
            continue;
        }

        // samplers
        D3D12_STATIC_SAMPLER_DESC samplers[2];
        
        ZeroMemory(&samplers[0], sizeof(samplers[0]));
        samplers[0].Filter = sharp ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS; // NEVER
            samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            samplers[0].MinLOD = 0.0f;
            samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
            samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//D3D12_SHADER_VISIBILITY_PIXEL;
            samplers[0].ShaderRegister = 0;

        samplers[1] = samplers[0];
        samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplers[1].ShaderRegister = 1;

        // constants and textures
        /*DECL_ARRAY(D3D12_ROOT_PARAMETER1) rootTables;
        ARRAY_INIT(rootTables);*/
        D3D12_ROOT_PARAMETER1 rootTables[2];
        // PS_CONSTANT_BUFFER - b0
        D3D12_DESCRIPTOR_RANGE1 range_PS_CONSTANT_BUFFER;
        ZeroMemory(&range_PS_CONSTANT_BUFFER, sizeof(D3D12_DESCRIPTOR_RANGE1));
        range_PS_CONSTANT_BUFFER.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        range_PS_CONSTANT_BUFFER.NumDescriptors = 1;
        range_PS_CONSTANT_BUFFER.BaseShaderRegister = PS_CONST_LUMI_BOUNDS;
        range_PS_CONSTANT_BUFFER.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        range_PS_CONSTANT_BUFFER.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


        
        D3D12_DESCRIPTOR_RANGE1 rangeTextureSRV;
        ZeroMemory(&rangeTextureSRV, sizeof(D3D12_DESCRIPTOR_RANGE1));
        rangeTextureSRV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeTextureSRV.NumDescriptors = shader_views[i];
        rangeTextureSRV.BaseShaderRegister = 0;
        rangeTextureSRV.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        rangeTextureSRV.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_DESCRIPTOR_TABLE1 table_PS_CONSTANT_BUFFER;
        ZeroMemory(&table_PS_CONSTANT_BUFFER, sizeof(D3D12_ROOT_DESCRIPTOR_TABLE1));
        table_PS_CONSTANT_BUFFER.NumDescriptorRanges = 1;
        table_PS_CONSTANT_BUFFER.pDescriptorRanges = &range_PS_CONSTANT_BUFFER;

        D3D12_ROOT_PARAMETER1 root_PS_CONSTANT_BUFFER;
        ZeroMemory(&root_PS_CONSTANT_BUFFER, sizeof(D3D12_ROOT_PARAMETER1));
        root_PS_CONSTANT_BUFFER.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_PS_CONSTANT_BUFFER.DescriptorTable = table_PS_CONSTANT_BUFFER;
        root_PS_CONSTANT_BUFFER.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//D3D12_SHADER_VISIBILITY_PIXEL; // TODO remap when changing ?
        rootTables[0] = root_PS_CONSTANT_BUFFER;
        

        D3D12_ROOT_DESCRIPTOR_TABLE1 table_TextureArray;
        ZeroMemory(&table_TextureArray, sizeof(D3D12_ROOT_DESCRIPTOR_TABLE1));
        table_TextureArray.NumDescriptorRanges = 1;
        table_TextureArray.pDescriptorRanges = &rangeTextureSRV;

        D3D12_ROOT_PARAMETER1 root_TextureArray;
        ZeroMemory(&root_TextureArray, sizeof(D3D12_ROOT_PARAMETER1));
        root_TextureArray.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_TextureArray.DescriptorTable = table_TextureArray;
        root_TextureArray.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;// D3D12_SHADER_VISIBILITY_PIXEL;

        rootTables[1] = root_TextureArray;
        

        
        D3D12_ROOT_SIGNATURE_DESC1 rootDesc;
        ZeroMemory(&rootDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC1));
        rootDesc.NumParameters = ARRAY_SIZE(rootTables);
        rootDesc.pParameters = rootTables;
        rootDesc.NumStaticSamplers = ARRAY_SIZE(samplers);
        rootDesc.pStaticSamplers = samplers;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignDesc;
        ZeroMemory(&rootSignDesc, sizeof(D3D12_VERSIONED_ROOT_SIGNATURE_DESC));
        rootSignDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignDesc.Desc_1_1 = rootDesc;

        ID3DBlob* signatureblob, * pErrBlob;
        hr = D3D12SerializeVersionedRootSignature(&rootSignDesc, &signatureblob, &pErrBlob);
        if (FAILED(hr))
        {
            char* err = (char* )pErrBlob->GetBufferPointer();
            //char* err = pErrBlob ? pErrBlob->GetBufferPointer() : NULL;
            
            if (pErrBlob)
                pErrBlob->Release();
            goto error;
        }
        

        hr = d3d_dev->CreateRootSignature( 0,
            signatureblob->GetBufferPointer(),
            signatureblob->GetBufferSize(),
            IID_ID3D12RootSignature,
            (void**)&pipelines[i].m_rootSignature);
        signatureblob->Release();
        if (FAILED(hr))
            goto error;
        D3D12_ObjectSetName(pipelines[i].m_rootSignature, L"quad Root Signature[%d]", i);

        // must match d3d_vertex_t
        static D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,  0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout.NumElements = ARRAY_SIZE(inputElementDescs);
        psoDesc.InputLayout.pInputElementDescs = inputElementDescs;
        psoDesc.pRootSignature = pipelines[i].m_rootSignature;
        psoDesc.VS.BytecodeLength = pVSBlob.buf_size;
        psoDesc.VS.pShaderBytecode = pVSBlob.buffer;
        psoDesc.PS.BytecodeLength = pPSBlob[i].buf_size;
        psoDesc.PS.pShaderBytecode = pPSBlob[i].buffer;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.SampleDesc.Count = 1;


        // D3D12_RASTERIZER_DESC
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // D3D12_BLEND_DESC
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (size_t i = 0; i < ARRAY_SIZE(psoDesc.BlendState.RenderTarget); i++)
        {
            ZeroMemory(&psoDesc.BlendState.RenderTarget[i], sizeof(D3D12_RENDER_TARGET_BLEND_DESC));
            psoDesc.BlendState.RenderTarget[i].BlendEnable = TRUE;
                psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

                psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
                psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_SRC_ALPHA; // keep source intact
                psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // RGB colors + inverse alpha (255 is full opaque)

                psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
                psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE; // keep source intact
                psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO; // discard

                psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
                psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        };

        static_assert(ARRAY_SIZE(display->pixelFormat->resourceFormat) <= ARRAY_SIZE(psoDesc.RTVFormats),
            "too many render targets");
        for (size_t j = 0; j < ARRAY_SIZE(display->pixelFormat->resourceFormat); j++)
        {
            if (display->pixelFormat->resourceFormat[j] == DXGI_FORMAT_UNKNOWN)
                break;
            psoDesc.RTVFormats[j] = display->pixelFormat->resourceFormat[j];
            psoDesc.NumRenderTargets++;
        }

        hr = d3d_dev->CreateGraphicsPipelineState(&psoDesc, IID_ID3D12PipelineState, (void**)&pipelines[i].m_pipelineState);
        if (FAILED(hr))
            goto error;
        D3D12_ObjectSetName(pipelines[i].m_pipelineState, L"quad Pipeline State");

        hr = CommandListCreateGraphic(d3d_dev, &commands[i]);
    }

error:
    // TODO cleaning
    D3D_ShaderBlobRelease(&pPSBlob[0]);
    if (pPSBlob[1].buffer)
        D3D_ShaderBlobRelease(&pPSBlob[1]);
    
    D3D_ShaderBlobRelease(&pVSBlob);
    return hr;
}

HRESULT CD3D12Shaders::FenceInit(d3d12_fence_t* fence, ID3D12Device* d3d_dev)
{
    HRESULT hr;
    fence->fenceReached = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!!(fence->fenceReached == NULL))
        return S_FALSE;
    hr = d3d_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)&fence->d3dRenderFence);
    if (FAILED(hr))
        CloseHandle(fence->fenceReached);
    return hr;
}

void CD3D12Shaders::FenceRelease(d3d12_fence_t* fence)
{
    if (fence->d3dRenderFence)
    {
        fence->d3dRenderFence->Release();
        fence->d3dRenderFence = NULL;
        CloseHandle(fence->fenceReached);
        fence->fenceReached = NULL;
    }
}

void D3D12_FenceWaitForPreviousFrame(d3d12_fence_t* fence, ID3D12CommandQueue* commandQueue)
{
    if (fence->fenceCounter == UINT64_MAX)
        fence->fenceCounter = 0;
    else
        fence->fenceCounter++;

    ResetEvent(fence->fenceReached);
    fence->d3dRenderFence->SetEventOnCompletion(fence->fenceCounter, fence->fenceReached);
    commandQueue->Signal( fence->d3dRenderFence, fence->fenceCounter);

    WaitForSingleObject(fence->fenceReached, INFINITE);
}
