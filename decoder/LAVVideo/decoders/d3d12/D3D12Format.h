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



#include "d3d12.h"
#include "d3d12video.h"
#include "LAVPixFmtConverter.h"
#include <cstdint>

#define DXGI_MAX_SHADER_VIEW     4
#define DXGI_MAX_RENDER_TARGET   2 // for NV12/P010 we render Y and UV separately

#define DXGI_RGB_FORMAT  1
#define DXGI_YUV_FORMAT  2

#define DXGI_CHROMA_CPU 1
#define DXGI_CHROMA_GPU 2

#define PS_CONST_LUMI_BOUNDS   0
#define VS_CONST_VIEWPOINT     1
#define PS_TEXTURE_PLANE_START 2
#define FIELD_OF_VIEW_DEGREES_DEFAULT  80.f
#define FIELD_OF_VIEW_DEGREES_MAX 150.f
#define FIELD_OF_VIEW_DEGREES_MIN 20.f
#define SPHERE_RADIUS 1.f

#define SPHERE_SLICES 128

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)
#define DEFAULT_BRIGHTNESS         100
#define DEFAULT_SRGB_BRIGHTNESS    100
#define MAX_HLG_BRIGHTNESS        1000
#define MAX_PQ_BRIGHTNESS        10000

typedef struct
{
    ID3D12RootSignature* m_rootSignature;
    ID3D12PipelineState* m_pipelineState;
} d3d12_pipeline_t;

typedef enum video_color_space_t
{
    COLOR_SPACE_UNDEF,
    COLOR_SPACE_BT601,
    COLOR_SPACE_BT709,
    COLOR_SPACE_BT2020,
#define COLOR_SPACE_SRGB      COLOR_SPACE_BT709
#define COLOR_SPACE_SMPTE_170 COLOR_SPACE_BT601
#define COLOR_SPACE_SMPTE_240 COLOR_SPACE_SMPTE_170
#define COLOR_SPACE_MAX       COLOR_SPACE_BT2020
} video_color_space_t;

typedef struct d3d_shader_blob
{
    void* opaque;
    void (*pf_release)(struct d3d_shader_blob*);
    SIZE_T buf_size;
    void* buffer;
} d3d_shader_blob;

//void D3D12_ObjectSetName(ID3D12Object*, const wchar_t* fmt, ...);
static void D3D12_ObjectSetName(ID3D12Object* o, const wchar_t* name, ...)
{
    wchar_t tmp[256];
    va_list ap;
    va_start(ap, name);
    vswprintf_s(tmp, ARRAY_SIZE(tmp), name, ap);
    va_end(ap);
    o->SetName(tmp);
}

//#define D3D12_ObjectSetName(o,s, ...)  D3D12_ObjectSetName((ID3D12Object*)o, L"lavfilter " s, ##__VA_ARGS__)

typedef enum video_orientation_t
{
    ORIENT_TOP_LEFT = 0, /**< Top line represents top, left column left. */
    ORIENT_TOP_RIGHT, /**< Flipped horizontally */
    ORIENT_BOTTOM_LEFT, /**< Flipped vertically */
    ORIENT_BOTTOM_RIGHT, /**< Rotated 180 degrees */
    ORIENT_LEFT_TOP, /**< Transposed */
    ORIENT_LEFT_BOTTOM, /**< Rotated 90 degrees anti-clockwise */
    ORIENT_RIGHT_TOP, /**< Rotated 90 degrees clockwise */
    ORIENT_RIGHT_BOTTOM, /**< Anti-transposed */
#define ORIENT_MAX ((size_t)ORIENT_RIGHT_BOTTOM)

    ORIENT_NORMAL = ORIENT_TOP_LEFT,
    ORIENT_TRANSPOSED = ORIENT_LEFT_TOP,
    ORIENT_ANTI_TRANSPOSED = ORIENT_RIGHT_BOTTOM,
    ORIENT_HFLIPPED = ORIENT_TOP_RIGHT,
    ORIENT_VFLIPPED = ORIENT_BOTTOM_LEFT,
    ORIENT_ROTATED_180 = ORIENT_BOTTOM_RIGHT,
    ORIENT_ROTATED_270 = ORIENT_LEFT_BOTTOM,
    ORIENT_ROTATED_90 = ORIENT_RIGHT_TOP,
} video_orientation_t;

typedef enum video_color_primaries_t
{
    COLOR_PRIMARIES_UNDEF,
    COLOR_PRIMARIES_BT601_525,
    COLOR_PRIMARIES_BT601_625,
    COLOR_PRIMARIES_BT709,
    COLOR_PRIMARIES_BT2020,
    COLOR_PRIMARIES_DCI_P3,
    COLOR_PRIMARIES_FCC1953,
    #define COLOR_PRIMARIES_SRGB            COLOR_PRIMARIES_BT709
    #define COLOR_PRIMARIES_SMTPE_170       COLOR_PRIMARIES_BT601_525
    #define COLOR_PRIMARIES_SMTPE_240       COLOR_PRIMARIES_BT601_525
    #define COLOR_PRIMARIES_SMTPE_RP145     COLOR_PRIMARIES_BT601_525
    #define COLOR_PRIMARIES_EBU_3213        COLOR_PRIMARIES_BT601_625
    #define COLOR_PRIMARIES_BT470_BG        COLOR_PRIMARIES_BT601_625
    #define COLOR_PRIMARIES_BT470_M         COLOR_PRIMARIES_FCC1953
    #define COLOR_PRIMARIES_MAX             COLOR_PRIMARIES_FCC1953
} video_color_primaries_t;

typedef enum video_transfer_func_t
{
    TRANSFER_FUNC_UNDEF,
    TRANSFER_FUNC_LINEAR,
    TRANSFER_FUNC_SRGB /*< Gamma 2.2 */,
    TRANSFER_FUNC_BT470_BG,
    TRANSFER_FUNC_BT470_M,
    TRANSFER_FUNC_BT709,
    TRANSFER_FUNC_SMPTE_ST2084,
    TRANSFER_FUNC_SMPTE_240,
    TRANSFER_FUNC_HLG,
    #define TRANSFER_FUNC_BT2020            TRANSFER_FUNC_BT709
    #define TRANSFER_FUNC_SMPTE_170         TRANSFER_FUNC_BT709
    #define TRANSFER_FUNC_SMPTE_274         TRANSFER_FUNC_BT709
    #define TRANSFER_FUNC_SMPTE_293         TRANSFER_FUNC_BT709
    #define TRANSFER_FUNC_SMPTE_296         TRANSFER_FUNC_BT709
    #define TRANSFER_FUNC_ARIB_B67          TRANSFER_FUNC_HLG
    #define TRANSFER_FUNC_MAX               TRANSFER_FUNC_HLG
} video_transfer_func_t;

struct xy_primary {
    double x, y;
};

struct cie1931_primaries {
    struct xy_primary red, green, blue, white;
};
#define CIE_D65 {0.31271, 0.32902}
#define CIE_C   {0.31006, 0.31616}

const cie1931_primaries COLOR_PRIMARIES_BT601_525_S[] = { {0.630,0.340},{0.310, 0.595},{0.155, 0.070},
        {CIE_D65} };

/*static const struct cie1931_primaries STANDARD_PRIMARIES[] = {


    [COLOR_PRIMARIES_BT601_525] = {
        .red = {0.630, 0.340},
        .green = {0.310, 0.595},
        .blue = {0.155, 0.070},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT601_625] = {
        .red = {0.640, 0.330},
        .green = {0.290, 0.600},
        .blue = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT709] = {
        .red = {0.640, 0.330},
        .green = {0.300, 0.600},
        .blue = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT2020] = {
        .red = {0.708, 0.292},
        .green = {0.170, 0.797},
        .blue = {0.131, 0.046},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_DCI_P3] = {
        .red = {0.680, 0.320},
        .green = {0.265, 0.690},
        .blue = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_FCC1953] = {
        .red = {0.670, 0.330},
        .green = {0.210, 0.710},
        .blue = {0.140, 0.080},
        .white = CIE_C
    },
#undef CIE_D65
#undef CIE_C
};*/

typedef struct
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layoutplane[2];
    UINT64 pitch_plane[2];
    UINT rows_plane[2];
    UINT64 RequiredSize;
} d3d12_footprint_t;

typedef struct
{
    const char* name;
    DXGI_FORMAT  formatTexture;
    uint32_t fourcc;
    uint8_t      bitsPerChannel;
    uint8_t      widthDenominator;
    uint8_t      heightDenominator;
    DXGI_FORMAT  resourceFormat[DXGI_MAX_SHADER_VIEW];
} d3d_format_t;

typedef struct {
    video_color_primaries_t  primaries;
    video_transfer_func_t    transfer;
    video_color_space_t      color;
    bool                     b_full_range;
    unsigned                 luminance_peak;
    const d3d_format_t* pixelFormat;
} display_info_t;

 /* structures passed to the pixel shader */
typedef struct {
    FLOAT Colorspace[4 * 3];
    FLOAT Opacity;
    FLOAT LuminanceScale;
    FLOAT BoundaryX;
    FLOAT BoundaryY;
    FLOAT padding[48]; // 256 bytes alignment
} PS_CONSTANT_BUFFER;

typedef struct {
    FLOAT View[4 * 4];
    FLOAT Zoom[4 * 4];
    FLOAT Projection[4 * 4];
    FLOAT padding[16]; // 256 bytes alignment
} VS_PROJECTION_CONST;



typedef struct
{
    ID3D12CommandAllocator* m_commandAllocator;
    union {
        ID3D12GraphicsCommandList* m_commandList;
        ID3D12VideoDecodeCommandList* m_decoderList;
    };
} d3d12_commands_t;

typedef bool (*d3d12_select_plane_t)(void* opaque, size_t plane_index, struct vlc_d3d12_plane*);

union DXGI_Color
{
    struct {
        FLOAT r, g, b, a;
    };
    struct {
        FLOAT y;
    };
    struct {
        FLOAT u, v;
    };
    FLOAT array[4];
};

struct vlc_d3d12_plane {
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor; // CPU descriptor of the plane render target
    ID3D12Resource* renderTarget; // the resource to render plane into
    union DXGI_Color            background; // background values for that plane (0,0,0,1 for RGBA black)
};

typedef struct {
    struct {
        FLOAT x;
        FLOAT y;
        FLOAT z;
    } position;
    struct {
        FLOAT uv[2];
    } texture;
} d3d_vertex_t;



typedef struct
{
    const char* name;
    DXGI_FORMAT  format;
    LAVOutPixFmts lav_format;
} dxgi_format_t;

typedef enum video_projection_mode_t
{
    PROJECTION_MODE_RECTANGULAR = 0,
    PROJECTION_MODE_EQUIRECTANGULAR,

    PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD = 0x100,
} video_projection_mode_t;

static inline bool is_d3d12_opaque(uint32_t chroma)
{
    return chroma == MAKEFOURCC('D', '2', '1', '1') ||
        chroma == MAKEFOURCC('D', '2', '1', '0') ||
        chroma == MAKEFOURCC('D', '2', 'R', 'G') ||
        chroma == MAKEFOURCC('D', '2', 'G', 'R');
}

typedef struct
{
    ID3D12Resource* resource[DXGI_MAX_SHADER_VIEW];
    unsigned                  resource_plane[DXGI_MAX_SHADER_VIEW];
    unsigned                  slice_index;
    unsigned                  array_size;
} picture_sys_d3d12_t;

typedef struct {
    ID3D12DescriptorHeap* heap;
    UINT                        m_descriptorSize;
} d3d12_heap_t;



typedef enum video_color_range_t
{
    COLOR_RANGE_UNDEF,
    COLOR_RANGE_FULL,
    COLOR_RANGE_LIMITED,
#define COLOR_RANGE_STUDIO COLOR_RANGE_LIMITED
#define COLOR_RANGE_MAX    COLOR_RANGE_LIMITED
} video_color_range_t;

typedef enum video_chroma_location_t
{
    CHROMA_LOCATION_UNDEF,
    CHROMA_LOCATION_LEFT,   /*< Most common in MPEG-2 Video, H.264/265 */
    CHROMA_LOCATION_CENTER, /*< Most common in MPEG-1 Video, JPEG */
    CHROMA_LOCATION_TOP_LEFT,
    CHROMA_LOCATION_TOP_CENTER,
    CHROMA_LOCATION_BOTTOM_LEFT,
    CHROMA_LOCATION_BOTTOM_CENTER,
#define CHROMA_LOCATION_MAX CHROMA_LOCATION_BOTTOM_CENTER
} video_chroma_location_t;

struct vlc_viewpoint_t {
    float yaw;   /* yaw in degrees */
    float pitch; /* pitch in degrees */
    float roll;  /* roll in degrees */
    float fov;   /* field of view in degrees */
};
typedef struct {
    unsigned num, den;
} vlc_rational_t;

typedef struct {
    unsigned plane_count;
    struct {
        vlc_rational_t w;
        vlc_rational_t h;
    } p[4];
    unsigned pixel_size;        /* Number of bytes per pixel for a plane */
    unsigned pixel_bits;        /* Number of bits actually used bits per pixel for a plane */
} vlc_chroma_description_t;

struct video_format_t
{
    uint32_t i_chroma;                               /**< picture chroma */

    unsigned int i_width;                                 /**< picture width */
    unsigned int i_height;                               /**< picture height */
    unsigned int i_x_offset;               /**< start offset of visible area */
    unsigned int i_y_offset;               /**< start offset of visible area */
    unsigned int i_visible_width;                 /**< width of visible area */
    unsigned int i_visible_height;               /**< height of visible area */

    unsigned int i_bits_per_pixel;             /**< number of bits per pixel */

    unsigned int i_sar_num;                   /**< sample/pixel aspect ratio */
    unsigned int i_sar_den;

    unsigned int i_frame_rate;                     /**< frame rate numerator */
    unsigned int i_frame_rate_base;              /**< frame rate denominator */

    uint32_t i_rmask, i_gmask, i_bmask;      /**< color masks for RGB chroma */
    //video_palette_t* p_palette;              /**< video palette from demuxer */
    video_orientation_t orientation;                /**< picture orientation */
    video_color_primaries_t primaries;                  /**< color primaries */
    video_transfer_func_t transfer;                   /**< transfer function */
    video_color_space_t space;                        /**< YCbCr color space */
    video_color_range_t color_range;            /**< 0-255 instead of 16-235 */
    video_chroma_location_t chroma_location;      /**< YCbCr chroma location */

    //video_multiview_mode_t multiview_mode;        /** Multiview mode, 2D, 3D */
    bool b_multiview_right_eye_first;   /** Multiview left or right eye first*/

    video_projection_mode_t projection_mode;            /**< projection mode */
    vlc_viewpoint_t pose;
    struct {
        /* similar to SMPTE ST 2086 mastering display color volume */
        uint16_t primaries[3 * 2]; /* G,B,R / x,y */
        uint16_t white_point[2]; /* x,y */
        uint32_t max_luminance;
        uint32_t min_luminance;
    } mastering;
    struct {
        /* similar to CTA-861.3 content light level */
        uint16_t MaxCLL;  /* max content light level */
        uint16_t MaxFALL; /* max frame average light level */
    } lighting;
    uint32_t i_cubemap_padding; /**< padding in pixels of the cube map faces */
};

typedef struct
{
    ID3D12Fence* d3dRenderFence;
    UINT64                   fenceCounter;
    HANDLE                   fenceReached;
} d3d12_fence_t;

static bool DxgiIsRGBFormat(const d3d_format_t* cfg)
{
    return cfg->resourceFormat[0] != DXGI_FORMAT_R8_UNORM &&
        cfg->resourceFormat[0] != DXGI_FORMAT_R16_UNORM &&
        cfg->formatTexture != DXGI_FORMAT_YUY2 &&
        cfg->formatTexture != DXGI_FORMAT_AYUV &&
        cfg->formatTexture != DXGI_FORMAT_Y210 &&
        cfg->formatTexture != DXGI_FORMAT_Y410 &&
        cfg->formatTexture != DXGI_FORMAT_420_OPAQUE;
}

static HRESULT D3D12_CommandListCreateGraphic(ID3D12Device* d3d_dev, d3d12_commands_t* cmd)
{
    HRESULT hr;
    hr = d3d_dev->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_ID3D12CommandAllocator, (void**)&cmd->m_commandAllocator);
    if (FAILED(hr))
    {
        //msg_Err(o, "failed to create quad command allocator. (hr=0x%lX)", hr);
        goto error;
    }
    D3D12_ObjectSetName(cmd->m_commandAllocator, L"quad Command Allocator");

    hr = d3d_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        cmd->m_commandAllocator, NULL/*pipelines[i].m_pipelineState*/,
        IID_ID3D12GraphicsCommandList, (void**)&cmd->m_commandList);
    if (FAILED(hr))
    {
        //msg_Err(o, "failed to create command list. (hr=0x%lX)", hr);
        goto error;
    }
    D3D12_ObjectSetName(cmd->m_commandList, L"quad Command List");

    // a command list is recording commands by default
    hr = cmd->m_commandList->Close();
    if (!!(FAILED(hr)))
    {
        //msg_Err(o, "failed to close command list. (hr=0x%lX)", hr);
        goto error;
    }
    return hr;
error:
    return hr;
}

class CD3D12Format
{
public:
    CD3D12Format();
    ~CD3D12Format();
    HRESULT DeviceSupportsFormat(ID3D12Device* d3d_dev, DXGI_FORMAT format, UINT supportFlags)
    {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support;
        support.Format = format;

        if (FAILED(d3d_dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
            &support, sizeof(support))))
            return S_FALSE;
        if ((support.Support1 & supportFlags) == supportFlags)
            return S_OK;
        return S_FALSE;
    }
    const d3d_format_t* FindD3D12Format(ID3D12Device* d3d_dev,
        uint32_t i_src_chroma,
        int rgb_yuv,
        uint8_t bits_per_channel,
        uint8_t widthDenominator,
        uint8_t heightDenominator,
        int cpu_gpu,
        UINT supportFlags);

    const d3d_format_t* D3D12_FindDXGIFormat(DXGI_FORMAT dxgi);


    const d3d_format_t* DxgiGetRenderFormatList(void);
private:

protected:

};
