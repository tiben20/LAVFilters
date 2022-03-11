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
#include "D3D12Format.h"

static const d3d_format_t d3d_formats[] = {
    { "NV12",     DXGI_FORMAT_NV12,           MAKEFOURCC('N','V','1','2') ,              8, 2, 2, {DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM}},
    { "VA_NV12",  DXGI_FORMAT_NV12,           MAKEFOURCC('D','X','1','1'),      8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "VA_NV12",  DXGI_FORMAT_NV12,           MAKEFOURCC('D','2','1','1'),      8, 2, 2, { DXGI_FORMAT_R8_UNORM,       DXGI_FORMAT_R8G8_UNORM } },
    { "P010",     DXGI_FORMAT_P010,           MAKEFOURCC('P','0','1','0'),             10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_P010",  DXGI_FORMAT_P010,           MAKEFOURCC('D','X','1','0'), 10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_P010",  DXGI_FORMAT_P010,           MAKEFOURCC('D','2','1','0'), 10, 2, 2, { DXGI_FORMAT_R16_UNORM,      DXGI_FORMAT_R16G16_UNORM } },
    { "VA_AYUV",  DXGI_FORMAT_AYUV,           MAKEFOURCC('D','X','1','1'),      8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_AYUV",  DXGI_FORMAT_AYUV,           MAKEFOURCC('D','2','1','1'),      8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "YUY2",     DXGI_FORMAT_YUY2,           MAKEFOURCC('Y','U','Y','2'),              8, 1, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_YUY2",  DXGI_FORMAT_YUY2,           MAKEFOURCC('D','X','1','1'),      8, 1, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_YUY2",  DXGI_FORMAT_YUY2,           MAKEFOURCC('D','2','1','1'),      8, 1, 2, { DXGI_FORMAT_R8G8B8A8_UNORM } },
#if 0
    { "Y416",     DXGI_FORMAT_Y416,           VLC_CODEC_I444_16L,     16, 1, 1, { DXGI_FORMAT_R16G16B16A16_UINT } },
#endif
    { "VA_Y210",  DXGI_FORMAT_Y210,           MAKEFOURCC('D','X','1','0'), 10, 1, 2, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "VA_Y210",  DXGI_FORMAT_Y210,           MAKEFOURCC('D','2','1','0'), 10, 1, 2, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "VA_Y410",  DXGI_FORMAT_Y410,           MAKEFOURCC('D','X','1','0'), 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "VA_Y410",  DXGI_FORMAT_Y410,           MAKEFOURCC('D','2','1','0'), 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
#if 0
    { "Y210",     DXGI_FORMAT_Y210,           VLC_CODEC_I422_10L,     10, 1, 2, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "Y410",     DXGI_FORMAT_Y410,           VLC_CODEC_I444,         10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "NV11",     DXGI_FORMAT_NV11,           VLC_CODEC_I411,          8, 4, 1, { DXGI_FORMAT_R8_UNORM,           DXGI_FORMAT_R8G8_UNORM} },
#endif
    { "I420",     DXGI_FORMAT_UNKNOWN,        MAKEFOURCC('I','4','2','0'),          8, 2, 2, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I420_10",  DXGI_FORMAT_UNKNOWN,        MAKEFOURCC('I','0','A','L'),     10, 2, 2, { DXGI_FORMAT_R16_UNORM,     DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM } },
    { "YUVA",     DXGI_FORMAT_UNKNOWN,        MAKEFOURCC('Y','U','V','A'),          8, 1, 1, { DXGI_FORMAT_R8_UNORM,      DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM } },
    { "I444_16",  DXGI_FORMAT_UNKNOWN,        MAKEFOURCC('I','4','F','L'),     16, 1, 1, { DXGI_FORMAT_R16_UNORM,     DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM } },
    { "B8G8R8A8", DXGI_FORMAT_B8G8R8A8_UNORM, MAKEFOURCC('B','G','R','A'),          8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, MAKEFOURCC('D','A','G','R'),  8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "VA_BGRA",  DXGI_FORMAT_B8G8R8A8_UNORM, MAKEFOURCC('D','2','G','R'),  8, 1, 1, { DXGI_FORMAT_B8G8R8A8_UNORM } },
    { "R8G8B8A8", DXGI_FORMAT_R8G8B8A8_UNORM, MAKEFOURCC('R','G','B','A'),          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, MAKEFOURCC('D','A','G','R'),  8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "VA_RGBA",  DXGI_FORMAT_R8G8B8A8_UNORM, MAKEFOURCC('D','2','R','G'),  8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "R8G8B8X8", DXGI_FORMAT_B8G8R8X8_UNORM, MAKEFOURCC('R','V','3','2'),         8, 1, 1, { DXGI_FORMAT_B8G8R8X8_UNORM } },
    { "RGBA64",   DXGI_FORMAT_R16G16B16A16_UNORM, MAKEFOURCC('R','G','A','4'),   16, 1, 1, { DXGI_FORMAT_R16G16B16A16_UNORM } },
    { "RGB10A2",  DXGI_FORMAT_R10G10B10A2_UNORM, MAKEFOURCC('R','G','A','0'),    10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "VA_RGB10", DXGI_FORMAT_R10G10B10A2_UNORM, MAKEFOURCC('D','X','R','G'), 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "VA_RGB10", DXGI_FORMAT_R10G10B10A2_UNORM, MAKEFOURCC('D','2','R','G'), 10, 1, 1, { DXGI_FORMAT_R10G10B10A2_UNORM } },
    { "AYUV",     DXGI_FORMAT_AYUV,           MAKEFOURCC('V','U','Y','A'),          8, 1, 1, { DXGI_FORMAT_R8G8B8A8_UNORM } },
    { "B5G6R5",   DXGI_FORMAT_B5G6R5_UNORM,   MAKEFOURCC('R','V','1','6'),         5, 1, 1, { DXGI_FORMAT_B5G6R5_UNORM } },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,  MAKEFOURCC('D','X','1','1'),  8, 2, 2, { DXGI_FORMAT_UNKNOWN } },
    { "I420_OPAQUE", DXGI_FORMAT_420_OPAQUE,  MAKEFOURCC('D','2','1','1'),  8, 2, 2, { DXGI_FORMAT_UNKNOWN } },

    //{ NULL, 0, 0, 0, 0, 0, {} }
};

CD3D12Format::CD3D12Format()
{
}

CD3D12Format::~CD3D12Format()
{
}

const d3d_format_t* CD3D12Format::FindD3D12Format(ID3D12Device* d3d_dev, uint32_t i_src_chroma, int rgb_yuv, uint8_t bits_per_channel, uint8_t widthDenominator, uint8_t heightDenominator, int cpu_gpu, UINT supportFlags)
{
    supportFlags |= D3D12_FORMAT_SUPPORT1_TEXTURE2D;
    for (const d3d_format_t* output_format = DxgiGetRenderFormatList();
        output_format->name != NULL; ++output_format)
    {
        if (i_src_chroma && i_src_chroma != output_format->fourcc)
            continue;
        if (bits_per_channel && bits_per_channel > output_format->bitsPerChannel)
            continue;
        int cpu_gpu_fmt = is_d3d12_opaque(output_format->fourcc) ? DXGI_CHROMA_GPU : DXGI_CHROMA_CPU;
        if ((cpu_gpu & cpu_gpu_fmt) == 0)
            continue;
        //int format = vlc_fourcc_IsYUV(output_format->fourcc) ? DXGI_YUV_FORMAT : DXGI_RGB_FORMAT;
        //if ((rgb_yuv & format) == 0)
        //    continue;
        if (widthDenominator && widthDenominator < output_format->widthDenominator)
            continue;
        if (heightDenominator && heightDenominator < output_format->heightDenominator)
            continue;

        DXGI_FORMAT textureFormat;
        if (output_format->formatTexture == DXGI_FORMAT_UNKNOWN)
            textureFormat = output_format->resourceFormat[0];
        else
            textureFormat = output_format->formatTexture;

        if (DeviceSupportsFormat(d3d_dev, textureFormat, supportFlags))
            return output_format;
    }
    return NULL;
    return nullptr;
}


const d3d_format_t* CD3D12Format::D3D12_FindDXGIFormat(DXGI_FORMAT dxgi)
{
    for (const d3d_format_t* output_format = DxgiGetRenderFormatList();
        output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == dxgi &&
            is_d3d12_opaque(output_format->fourcc))
        {
            return output_format;
        }
    }
    return NULL;
}
const d3d_format_t* CD3D12Format::DxgiGetRenderFormatList(void)
{
    return d3d_formats;
}
