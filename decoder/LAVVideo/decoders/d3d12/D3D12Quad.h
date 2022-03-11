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


#include <vector>

#include "D3D12Format.h"
#include "D3D12Shaders.h"
#include "D3D12SwapChain.h"

typedef struct {
    const char* name;
    const GUID* guid;
    int           bit_depth;
    struct {
        uint8_t log2_chroma_w;
        uint8_t log2_chroma_h;
    };
    enum AVCodecID codec;
    const int* p_profiles; // NULL or ends with 0
    int           workaround;
} d3d_mode_t;

typedef struct
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layoutplane[2];
    UINT64 pitch;
    UINT64 pitch_plane[2];
    UINT rows;
    UINT rows_plane[2];
    UINT64 RequiredSize;
} d3d12_footprint_t;

class CD3D12Quad
{
public:
    CD3D12Quad(const d3d_format_t* fmt);
    ~CD3D12Quad();
    int D3D12_AllocateQuad(ID3D12Device* d3d_dev, video_projection_mode_t projection);
    void CreateShaderView(ID3D12Device* d3d_dev, ID3D12Resource* staging, d3d12_footprint_t footprint);
    void D3D12_RenderQuad(ID3D12Device* d3d_dev, picture_sys_d3d12_t* picsys,
        std::vector<ID3D12CommandList*> commandLists,
        vlc_d3d12_plane params[2]); //int (*get_buffer2)(struct AVCodecContext* s, AVFrame* frame, int flags)
    //(void* opaque, size_t plane, void* out)
    bool AllocQuadVertices(ID3D12Device* d3d_dev, video_projection_mode_t proj);
    void ReleaseQuad();

    int SetupQuad(const video_format_t* fmt, const display_info_t* displayFormat);
    void SetupQuadFlat(d3d_vertex_t* dst_data, const RECT* output, WORD* triangle_pos, video_orientation_t orientation);
    void UpdateViewport(const RECT* rect, const d3d_format_t* display);
    void orientationVertexOrder(video_orientation_t orientation, int vertex_order[4]);
    void UpdateQuadOpacity(float opacity);
    void UpdateQuadLuminanceScale(float luminanceScale);
    void UpdateViewpoint(const vlc_viewpoint_t* viewpoint, float f_sar);
    bool UpdateQuadPosition(const RECT* output, video_orientation_t orientation);

    void ReleasePipeline(d3d12_pipeline_t* pipeline);
    void ReleasePixelShader();
    unsigned int              i_width;
    unsigned int              i_height;
    const d3d_format_t* textureFormat;

    d3d12_commands_t          commands[DXGI_MAX_RENDER_TARGET];
    d3d12_pipeline_t          m_pipelines[DXGI_MAX_RENDER_TARGET];
    size_t                    shader_views[DXGI_MAX_RENDER_TARGET];

    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE GetShaderGpuHeapStart();
private:
    ID3D12Resource* m_vertexBuffer;
    ID3D12Resource* pIndexBuffer;
    ID3D12Resource* viewpointShaderConstant;
    ID3D12Resource* pPixelShaderConstants;
    
    D3D12_VIEWPORT            cropViewport[DXGI_MAX_SHADER_VIEW];
    D3D12_RECT                m_scissorRect[DXGI_MAX_SHADER_VIEW];

    D3D12_VERTEX_BUFFER_VIEW  m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW   m_indexBufferView;

    d3d12_heap_t              m_cbvSrvHeap;
    ID3D12Resource* resourceView;

    ID3D12DescriptorHeap* m_pSrvHeap;
    int                   m_pSrvIncrement;
    
    
    UINT                      vertexCount;
    UINT                      vertexStride;
    UINT                      indexCount;
    video_projection_mode_t   projection;

    

    PS_CONSTANT_BUFFER* shaderConstants;
    VS_PROJECTION_CONST* vertexConstants;
protected:

};
