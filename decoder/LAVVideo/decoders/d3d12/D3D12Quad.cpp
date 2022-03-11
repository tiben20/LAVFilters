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
#include "D3D12Quad.h"
#include "d3dx12.h"
CD3D12Quad::CD3D12Quad(const d3d_format_t* fmt)
{
    textureFormat = fmt;
    shader_views[0] = 0;// malloc(sizeof(shader_views));
    shader_views[1] = 0;
    shader_views[2] = 0;
    m_pSrvHeap = nullptr;
    m_pSrvIncrement = 0;
}

CD3D12Quad::~CD3D12Quad()
{
}

void CD3D12Quad::CreateShaderView(ID3D12Device* d3d_dev, ID3D12Resource* staging, d3d12_footprint_t m_pStagingLayout)
{
    D3D12_RESOURCE_DESC stagedesc = staging->GetDesc();
    //nv12
    //resource_format[0] = DXGI_FORMAT_R8_UNORM;
    //resource_format[1] = DXGI_FORMAT_R8G8_UNORM;
    //num_resources = 2;
    DXGI_FORMAT formats[2];
    formats[0] = DXGI_FORMAT_R8_UNORM;
    formats[1] = DXGI_FORMAT_R8G8_UNORM;
    m_pSrvIncrement = d3d_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = { };
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 2;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;
    
    HRESULT hr = d3d_dev->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_pSrvHeap));
    CD3DX12_CPU_DESCRIPTOR_HANDLE srv_handle (m_pSrvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int plane = 0; plane < 2; plane++)
    {
        
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
        srv_desc.Format = formats[plane];
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2DArray.MipLevels = 1;
        srv_desc.Texture2DArray.FirstArraySlice = plane;
        srv_desc.Texture2DArray.ArraySize = 1;
        srv_desc.Texture2DArray.PlaneSlice = plane;//DXGI_FORMAT_R8G8_UNORM ? 1 : i;
        //srv_desc.Texture2D.ResourceMinLODClamp = 0xf;
        d3d_dev->CreateShaderResourceView(staging,
            &srv_desc, srv_handle);
        srv_handle.Offset(m_pSrvIncrement);
    }

}

D3D12_GPU_DESCRIPTOR_HANDLE CD3D12Quad::GetShaderGpuHeapStart()
{
    D3D12_GPU_DESCRIPTOR_HANDLE srvHeapDesc = {};
    srvHeapDesc = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pSrvHeap->GetGPUDescriptorHandleForHeapStart());
    return srvHeapDesc;
}

D3D12_CPU_DESCRIPTOR_HANDLE CD3D12Quad::GetShaderCpuHeapStart()
{
    D3D12_CPU_DESCRIPTOR_HANDLE srvHeapDesc = {};
    srvHeapDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pSrvHeap->GetCPUDescriptorHandleForHeapStart());
    return srvHeapDesc;

}

void CD3D12Quad::D3D12_RenderQuad(ID3D12Device* d3d_dev, picture_sys_d3d12_t* picsys,
    std::vector<ID3D12CommandList*> commandLists,
    vlc_d3d12_plane params[2])
{
    size_t plane;
    if (resourceView != picsys->resource[0])
    {
        resourceView = picsys->resource[0];
        size_t srv_handle = PS_TEXTURE_PLANE_START;
        for (size_t slice = 0; slice < picsys->array_size; slice++)
        {
            for (plane = 0; plane < DXGI_MAX_SHADER_VIEW; plane++)
            {
                if (!textureFormat->resourceFormat[plane])
                    break;
                // use the first texture when there's a plane with no texture
                // the shader expects DXGI_MAX_SHADER_VIEW textures but will use less
                // hopefully it has no impact on performance
                D3D12_SHADER_RESOURCE_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = textureFormat->resourceFormat[plane];
                desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipLevels = 1;
                desc.Texture2DArray.FirstArraySlice = slice; // picsys->slice_index; // all slices
                desc.Texture2DArray.ArraySize = 1;//picsys->array_size;
                desc.Texture2DArray.PlaneSlice = picsys->resource_plane[plane];

                d3d_dev->CreateShaderResourceView(picsys->resource[plane],
                    &desc, D3D12_HeapCPUHandle(&m_cbvSrvHeap, srv_handle));
                srv_handle++;
            }
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(commands); i++)
    {
        if (!m_pipelines[i].m_pipelineState)
            break;

        struct vlc_d3d12_plane renderTarget;
        renderTarget = params[i];
#if 0
        
        if (!!(!selectPlane(selectOpaque, i, &renderTarget)))
            continue;
#endif
        commands[i].m_commandAllocator->Reset();
        
        commands[i].m_commandList->Reset(commands[i].m_commandAllocator, m_pipelines[i].m_pipelineState);


        D3D12_RESOURCE_BARRIER barrier;
        ZeroMemory(&barrier, sizeof(barrier));
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        barrier.Transition.pResource = renderTarget.renderTarget,
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON,
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
        
            commands[i].m_commandList->ResourceBarrier( 1, &barrier);

        commands[i].m_commandList->ClearRenderTargetView(renderTarget.descriptor, renderTarget.background.array, 0, NULL);


        commands[i].m_commandList->SetGraphicsRootSignature(m_pipelines[i].m_rootSignature);

        commands[i].m_commandList->SetDescriptorHeaps( 1, &m_cbvSrvHeap.heap);

        // MUST appear in the same order as the rootTables
        // constant buffer
        commands[i].m_commandList->SetGraphicsRootDescriptorTable(0, D3D12_HeapGPUHandle(&m_cbvSrvHeap, PS_CONST_LUMI_BOUNDS));
        // texture slice start
        commands[i].m_commandList->SetGraphicsRootDescriptorTable( 1,
            D3D12_HeapGPUHandle(&m_cbvSrvHeap, PS_TEXTURE_PLANE_START + (shader_views[0] + shader_views[1]) * picsys->slice_index));
#ifdef TODO
        /* vertex shader */
        if (viewpointShaderConstant)
            ID3D11DeviceContext_VSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &viewpointShaderConstant);
#endif

        commands[i].m_commandList->RSSetViewports(1, &cropViewport[i]);
        commands[i].m_commandList->RSSetScissorRects(1, &m_scissorRect[i]);


        /* Render the quad */
        commands[i].m_commandList->OMSetRenderTargets( 1, &renderTarget.descriptor, TRUE, NULL);

        commands[i].m_commandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        commands[i].m_commandList->IASetVertexBuffers( 0, 1, &m_vertexBufferView);
        commands[i].m_commandList->IASetIndexBuffer( &m_indexBufferView);

        commands[i].m_commandList->DrawIndexedInstanced( indexCount, 1, 0, 0, 0);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        commands[i].m_commandList->ResourceBarrier( 1, &barrier);

        commands[i].m_commandList->Close();
        commandLists.push_back((ID3D12CommandList*)commands[i].m_commandList);
        
    }
}

bool CD3D12Quad::AllocQuadVertices(/*vlc_object_t* o,*/ ID3D12Device* d3d_dev, video_projection_mode_t proj)
{
    HRESULT hr;

    switch (proj)
    {
    case PROJECTION_MODE_RECTANGULAR:
        vertexCount = 4;
        indexCount = 2 * 3;
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        vertexCount = (128 + 1) * (128 + 1);
        indexCount = 128 * 128 * 2 * 3;
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        vertexCount = 4 * 6;
        indexCount = 6 * 2 * 3;
        break;
    default:
        return false;
    }

    vertexStride = sizeof(d3d_vertex_t);
    projection = projection;

    D3D12_HEAP_PROPERTIES constProp;
    ZeroMemory(&constProp, sizeof(constProp));
    constProp.Type = D3D12_HEAP_TYPE_UPLOAD,
        constProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        constProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        constProp.CreationNodeMask = 1;
        constProp.VisibleNodeMask = 1;
D3D12_RESOURCE_DESC constantDesc;
ZeroMemory(&constantDesc, sizeof(constantDesc));
constantDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
constantDesc.Width = vertexStride * vertexCount;
constantDesc.Height = 1;
constantDesc.DepthOrArraySize = 1;
constantDesc.MipLevels = 1;
constantDesc.Format = DXGI_FORMAT_UNKNOWN;
constantDesc.SampleDesc.Count = 1;
constantDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
constantDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = d3d_dev->CreateCommittedResource(&constProp, D3D12_HEAP_FLAG_NONE,
        &constantDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
        IID_ID3D12Resource, (void**)&m_vertexBuffer);
    if (FAILED(hr)) {

        goto fail;
    }
    
    D3D12_ObjectSetName(m_vertexBuffer, L"quad Vertex Buffer");

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = constantDesc.Width;
    m_vertexBufferView.StrideInBytes = vertexStride;

    constantDesc.Width = sizeof(WORD) * indexCount;
    hr = d3d_dev->CreateCommittedResource(&constProp, D3D12_HEAP_FLAG_NONE,
        &constantDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
        IID_ID3D12Resource, (void**)&pIndexBuffer);
    if (FAILED(hr)) {
        
        goto fail;
    }
    D3D12_ObjectSetName(pIndexBuffer, L"quad Index Buffer");

    m_indexBufferView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = constantDesc.Width;
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;

    return true;
fail:
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = NULL;
    }
    if (pIndexBuffer)
    {
        pIndexBuffer->Release();
        pIndexBuffer = NULL;
    }
    return false;
}

void CD3D12Quad::ReleaseQuad()
{
    if (pPixelShaderConstants)
    {
        if (shaderConstants)
        {
            pPixelShaderConstants->Unmap(0, NULL);
            shaderConstants = NULL;
        }
        pPixelShaderConstants->Release();
        pPixelShaderConstants = NULL;
    }
    if (viewpointShaderConstant)
    {
        if (vertexConstants)
        {
            viewpointShaderConstant->Unmap(0, NULL);
            vertexConstants = NULL;
        }
        viewpointShaderConstant->Release();
        viewpointShaderConstant = NULL;
    }

    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = NULL;
    }
    if (pIndexBuffer)
    {
        pIndexBuffer->Release();
        pIndexBuffer = NULL;
    }
    ReleasePixelShader();
}

bool CD3D12Quad::UpdateQuadPosition( const RECT* output, video_orientation_t orientation)
{
    bool result = true;
    HRESULT hr;
    d3d_vertex_t* pVertexDataBegin;

    if (!!(m_vertexBuffer == NULL))
        return false;

    D3D12_RANGE readRange = { 0 }; // no reading of buffers we write in

    /* create the vertices */
    hr = m_vertexBuffer->Map(0, &readRange, (void**)&pVertexDataBegin);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Failed to lock the vertex buffer"));
        return false;
    }

    void* triangleVertices;
    /* create the vertex indices */
    hr = pIndexBuffer->Map(0, & readRange, (void**)&triangleVertices);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Failed to lock the index buffer"));
        m_vertexBuffer->Unmap( 0, NULL);
        return false;
    }

    switch (projection)
    {
    case PROJECTION_MODE_RECTANGULAR:
        SetupQuadFlat(pVertexDataBegin, output, (WORD*)triangleVertices, orientation);
        break;
    /*case PROJECTION_MODE_EQUIRECTANGULAR:
        SetupQuadSphere(dst_data, output, quad, pData);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        SetupQuadCube(dst_data, output, quad, pData);
        break;*/
    default:
        DbgLog((LOG_TRACE, 10, L"Projection mode %d not handled"), projection);
        
        return false;
    }
    

    pIndexBuffer->Unmap(0, NULL);
    m_vertexBuffer->Unmap(0, NULL);

    return result;
}

void CD3D12Quad::orientationVertexOrder(video_orientation_t orientation, int vertex_order[4])
{
    switch (orientation) {
    case ORIENT_ROTATED_90:
        vertex_order[0] = 3;
        vertex_order[1] = 0;
        vertex_order[2] = 1;
        vertex_order[3] = 2;
        break;
    case ORIENT_ROTATED_270:
        vertex_order[0] = 1;
        vertex_order[1] = 2;
        vertex_order[2] = 3;
        vertex_order[3] = 0;
        break;
    case ORIENT_ROTATED_180:
        vertex_order[0] = 2;
        vertex_order[1] = 3;
        vertex_order[2] = 0;
        vertex_order[3] = 1;
        break;
    case ORIENT_TRANSPOSED:
        vertex_order[0] = 2;
        vertex_order[1] = 1;
        vertex_order[2] = 0;
        vertex_order[3] = 3;
        break;
    case ORIENT_HFLIPPED:
        vertex_order[0] = 1;
        vertex_order[1] = 0;
        vertex_order[2] = 3;
        vertex_order[3] = 2;
        break;
    case ORIENT_VFLIPPED:
        vertex_order[0] = 3;
        vertex_order[1] = 2;
        vertex_order[2] = 1;
        vertex_order[3] = 0;
        break;
    case ORIENT_ANTI_TRANSPOSED: /* transpose + vflip */
        vertex_order[0] = 0;
        vertex_order[1] = 3;
        vertex_order[2] = 2;
        vertex_order[3] = 1;
        break;
    default:
        vertex_order[0] = 0;
        vertex_order[1] = 1;
        vertex_order[2] = 2;
        vertex_order[3] = 3;
        break;
    }
}

void CD3D12Quad::SetupQuadFlat(d3d_vertex_t* dst_data, const RECT* output,
    WORD* triangle_pos, video_orientation_t orientation)
{
    unsigned int src_width = i_width;
    unsigned int src_height = i_height;
    float MidX, MidY;

    float top, bottom, left, right;
    /* find the middle of the visible part of the texture, it will be a 0,0
     * the rest of the visible area must correspond to -1,1 */
    switch (orientation)
    {
    case ORIENT_ROTATED_90: /* 90° anti clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top = MidY / (MidY - output->top);
        bottom = -(src_height - MidX) / (MidX - output->top);
        left = (MidX - src_height) / (MidX - output->left);
        right = MidX / (MidX - (src_width - output->right));
        break;
    case ORIENT_ROTATED_180: /* 180° */
        /* right/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top = (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left = -MidX / (MidX - output->left);
        right = (src_width - MidX) / (output->right - MidX);
        break;
    case ORIENT_ROTATED_270: /* 90° clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top = (src_width - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left = -MidX / (MidX - output->left);
        right = (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_ANTI_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top = (src_width - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left = -(src_height - MidY) / (output->bottom - MidY);
        right = MidX / (MidX - output->left);
        break;
    case ORIENT_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top = (src_width - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left = -MidX / (MidX - output->left);
        right = (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_VFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top = (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left = -MidX / (MidX - output->left);
        right = (src_width - MidX) / (output->right - MidX);
        break;
    case ORIENT_HFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top = MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left = -(src_width - MidX) / (output->right - MidX);
        right = MidX / (MidX - output->left);
        break;
    case ORIENT_NORMAL:
    default:
        /* left/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top = MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left = -MidX / (MidX - output->left);
        right = (src_width - MidX) / (output->right - MidX);
        break;
    }

    const float vertices_coords[4][2] = {
        { left,  bottom },
        { right, bottom },
        { right, top    },
        { left,  top    },
    };

    /* Compute index remapping necessary to implement the rotation. */
    int vertex_order[4];
    orientationVertexOrder(orientation, vertex_order);

    for (int i = 0; i < 4; ++i) {
        dst_data[i].position.x = vertices_coords[vertex_order[i]][0];
        dst_data[i].position.y = vertices_coords[vertex_order[i]][1];
    }

    // bottom left
    dst_data[0].position.z = 0.0f;
    dst_data[0].texture.uv[0] = 0.0f;
    dst_data[0].texture.uv[1] = 1.0f;

    // bottom right
    dst_data[1].position.z = 0.0f;
    dst_data[1].texture.uv[0] = 1.0f;
    dst_data[1].texture.uv[1] = 1.0f;

    // top right
    dst_data[2].position.z = 0.0f;
    dst_data[2].texture.uv[0] = 1.0f;
    dst_data[2].texture.uv[1] = 0.0f;

    // top left
    dst_data[3].position.z = 0.0f;
    dst_data[3].texture.uv[0] = 0.0f;
    dst_data[3].texture.uv[1] = 0.0f;

    /* Make sure surfaces are facing the right way */
    if (orientation == ORIENT_TOP_RIGHT || orientation == ORIENT_BOTTOM_LEFT
        || orientation == ORIENT_LEFT_TOP || orientation == ORIENT_RIGHT_BOTTOM)
    {
        triangle_pos[0] = 0;
        triangle_pos[1] = 1;
        triangle_pos[2] = 3;

        triangle_pos[3] = 3;
        triangle_pos[4] = 1;
        triangle_pos[5] = 2;
    }
    else
    {
        triangle_pos[0] = 3;
        triangle_pos[1] = 1;
        triangle_pos[2] = 0;

        triangle_pos[3] = 2;
        triangle_pos[4] = 1;
        triangle_pos[5] = 3;
    }
    }

#if 0
bool CD3D12Quad::ShaderUpdateConstants(vlc_object_t* o, ID3D12Device* d3d_dev, d3d12_quad_t* quad, int type, void* new_buf)
{
#if 0
    ID3D12Resource* res = viewpointShaderConstant;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, res, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (unlikely(FAILED(hr)))
    {
        msg_Err(o, "Failed to lock the picture shader constants (hr=0x%lX)", hr);
        return false;
    }

    memcpy(mappedResource.pData, new_buf, sizeof(*vertexConstants));

    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, res, 0);
#endif
    return true;
}
#endif
void CD3D12Quad::UpdateQuadOpacity(float opacity)
{
    shaderConstants->Opacity = opacity;
}

void CD3D12Quad::UpdateQuadLuminanceScale(float luminanceScale)
{
    shaderConstants->LuminanceScale = luminanceScale;
}

static float UpdateFOVy(float f_fovx, float f_sar)
{
    return 2 * atanf(tanf(f_fovx / 2) / f_sar);
}

static float UpdateZ(float f_fovx, float f_fovy)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(f_fovx / 2);
    float tan_fovy_2 = tanf(f_fovy / 2);
    float z_min = -SPHERE_RADIUS / sinf(atanf(sqrtf(
        tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    float f_z;
    if (f_fovx <= z_thresh * M_PI / 180)
        f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        f_z = f * f_fovx - f * z_thresh * M_PI / 180;
        if (f_z < z_min)
            f_z = z_min;
    }
    return f_z;
}
static void getZoomMatrix(float zoom, FLOAT matrix[16]) {

    const FLOAT m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, FLOAT matrix[16]) {

    float zFar = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const FLOAT m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f };

    memcpy(matrix, m, sizeof(m));
}

static void viewpoint_to_4x4(const vlc_viewpoint_t* vp, float* m)
{
    float yaw = -vp->yaw * (float)M_PI / 180.f;
    float pitch = -vp->pitch * (float)M_PI / 180.f;
    float roll = -vp->roll * (float)M_PI / 180.f;

    float s, c;

    s = sinf(pitch);
    c = cosf(pitch);
    const float x_rot[4][4] = {
        { 1.f,    0.f,    0.f,    0.f },
        { 0.f,    c,      -s,      0.f },
        { 0.f,    s,      c,      0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    s = sinf(yaw);
    c = cosf(yaw);
    const float y_rot[4][4] = {
        { c,      0.f,    s,     0.f },
        { 0.f,    1.f,    0.f,    0.f },
        { -s,      0.f,    c,      0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    s = sinf(roll);
    c = cosf(roll);
    const float z_rot[4][4] = {
        { c,      s,      0.f,    0.f },
        { -s,     c,      0.f,    0.f },
        { 0.f,    0.f,    1.f,    0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    /**
     * Column-major matrix multiplication mathematically equal to
     * z_rot * x_rot * y_rot
     */
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                for (int l = 0; l < 4; ++l)
                    m[4 * i + l] += y_rot[i][j] * x_rot[j][k] * z_rot[k][l];
}
void CD3D12Quad::UpdateViewpoint(const vlc_viewpoint_t* viewpoint, float f_sar)
{
    if (!viewpointShaderConstant)
        return;
    // Convert degree into radian
    float f_fovx = viewpoint->fov * (float)M_PI / 180.f;
    if (f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f ||
        f_fovx < -0.001f)
        return;

    float f_fovy = UpdateFOVy(f_fovx, f_sar);
    float f_z = UpdateZ(f_fovx, f_fovy);

    getZoomMatrix(SPHERE_RADIUS * f_z, vertexConstants->Zoom);
    getProjectionMatrix(f_sar, f_fovy, vertexConstants->Projection);
    viewpoint_to_4x4(viewpoint, vertexConstants->View);
    

    //ShaderUpdateConstants(o, d3d_dev, quad, VS_CONST_VIEWPOINT, &vertexConstants);
}

int CD3D12Quad::D3D12_AllocateQuad(ID3D12Device* d3d_dev,video_projection_mode_t projection)
{
    HRESULT hr;

    hr = D3D12_HeapInit(d3d_dev, &m_cbvSrvHeap,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        2 + DXGI_MAX_SHADER_VIEW * 64);
    // 2: shaderConstants + vertexConstants
    // DXGI_MAX_SHADER_VIEW * 64: texture resource * max array slices
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"Failed to create the CBV Heap"));
        goto error;
    }

    static_assert((sizeof(*shaderConstants) % 256) == 0, "Constant buffers require 256-byte alignment");
    D3D12_HEAP_PROPERTIES constProp;
    ZeroMemory(&constProp, sizeof(D3D12_HEAP_PROPERTIES));
    constProp.Type = D3D12_HEAP_TYPE_UPLOAD;
    constProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    constProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    constProp.CreationNodeMask = 1;
    constProp.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC constantDesc;
    ZeroMemory(&constantDesc, sizeof(D3D12_RESOURCE_DESC));
    constantDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    constantDesc.Width = sizeof(*shaderConstants);
    constantDesc.Height = 1;
    constantDesc.DepthOrArraySize = 1;
    constantDesc.MipLevels = 1;
    constantDesc.Format = DXGI_FORMAT_UNKNOWN;
    constantDesc.SampleDesc.Count = 1;
    constantDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    constantDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = d3d_dev->CreateCommittedResource(&constProp, D3D12_HEAP_FLAG_NONE,
        &constantDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
        IID_ID3D12Resource, (void**)&pPixelShaderConstants);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Could not create the pixel shader constant buffer. (hr=0x%lX)", hr));
        goto error;
    }
    D3D12_ObjectSetName(pPixelShaderConstants, L"quad PS_CONST_LUMI_BOUNDS");

    D3D12_RANGE readRange;
    ZeroMemory(&readRange, sizeof(D3D12_RANGE));
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    ZeroMemory(&cbvDesc, sizeof(D3D12_CONSTANT_BUFFER_VIEW_DESC));
    cbvDesc.BufferLocation = pPixelShaderConstants->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantDesc.Width;

    d3d_dev->CreateConstantBufferView(&cbvDesc, D3D12_HeapCPUHandle(&m_cbvSrvHeap, PS_CONST_LUMI_BOUNDS));
    hr = pPixelShaderConstants->Map(0, &readRange, (void**)&shaderConstants);
    if (FAILED(hr)) {
        DbgLog((LOG_TRACE, 10, L"Failed to map lumi bounds buffer. (hr=0x%lX)", hr));
        goto error;
    }

    if (projection == PROJECTION_MODE_EQUIRECTANGULAR || projection == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        static_assert((sizeof(*vertexConstants) % 256) == 0, "Constant buffers require 256-byte alignment");
        constantDesc.Width = sizeof(*vertexConstants);
        hr = d3d_dev->CreateCommittedResource(&constProp, D3D12_HEAP_FLAG_NONE,
            &constantDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
            IID_ID3D12Resource, (void**)&viewpointShaderConstant);
        if (FAILED(hr)) {
            DbgLog((LOG_TRACE, 10, L"Could not create the vertex shader constant buffer. (hr=0x%lX)", hr));
            goto error;
        }
        D3D12_ObjectSetName(viewpointShaderConstant, L"quad Viewpoint");

        cbvDesc.BufferLocation = viewpointShaderConstant->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantDesc.Width;
        d3d_dev->CreateConstantBufferView(&cbvDesc, D3D12_HeapCPUHandle(&m_cbvSrvHeap, VS_CONST_VIEWPOINT));
        hr = viewpointShaderConstant->Map(0, &readRange, (void**)&vertexConstants);
        if (FAILED(hr)) {
            DbgLog((LOG_TRACE, 10, L"Failed to map vertex constant buffer. (hr=0x%lX)", hr));
            goto error;
        }
    }

    if (!AllocQuadVertices(d3d_dev, projection))
        goto error;

    return 0;

error:
    ReleaseQuad();
    return -1;
}


float GetFormatLuminance(const video_format_t* fmt)
{
    switch (fmt->transfer)
    {
    case TRANSFER_FUNC_SMPTE_ST2084:
        /* that's the default PQ value if the metadata are not set */
        return MAX_PQ_BRIGHTNESS;
    case TRANSFER_FUNC_HLG:
        return MAX_HLG_BRIGHTNESS;
    case TRANSFER_FUNC_BT470_BG:
    case TRANSFER_FUNC_BT470_M:
    case TRANSFER_FUNC_BT709:
    case TRANSFER_FUNC_SRGB:
        return DEFAULT_BRIGHTNESS;
    default:
        
        return DEFAULT_BRIGHTNESS;
    }
}



static void Float3x3Inverse(double in_out[3 * 3])
{
    double m00 = in_out[0 + 0 * 3], m01 = in_out[1 + 0 * 3], m02 = in_out[2 + 0 * 3],
        m10 = in_out[0 + 1 * 3], m11 = in_out[1 + 1 * 3], m12 = in_out[2 + 1 * 3],
        m20 = in_out[0 + 2 * 3], m21 = in_out[1 + 2 * 3], m22 = in_out[2 + 2 * 3];

    // calculate the adjoint
    in_out[0 + 0 * 3] = (m11 * m22 - m21 * m12);
    in_out[1 + 0 * 3] = -(m01 * m22 - m21 * m02);
    in_out[2 + 0 * 3] = (m01 * m12 - m11 * m02);
    in_out[0 + 1 * 3] = -(m10 * m22 - m20 * m12);
    in_out[1 + 1 * 3] = (m00 * m22 - m20 * m02);
    in_out[2 + 1 * 3] = -(m00 * m12 - m10 * m02);
    in_out[0 + 2 * 3] = (m10 * m21 - m20 * m11);
    in_out[1 + 2 * 3] = -(m00 * m21 - m20 * m01);
    in_out[2 + 2 * 3] = (m00 * m11 - m10 * m01);

    // calculate the determinant (as inverse == 1/det * adjoint,
    // adjoint * m == identity * det, so this calculates the det)
    double det = m00 * in_out[0 + 0 * 3] + m10 * in_out[1 + 0 * 3] + m20 * in_out[2 + 0 * 3];
    det = 1.0f / det;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            in_out[j + i * 3] *= det;
    }
}


static void Float3x3Multiply(double m1[3 * 3], const double m2[3 * 3])
{
    double a00 = m1[0 + 0 * 3], a01 = m1[1 + 0 * 3], a02 = m1[2 + 0 * 3],
        a10 = m1[0 + 1 * 3], a11 = m1[1 + 1 * 3], a12 = m1[2 + 1 * 3],
        a20 = m1[0 + 2 * 3], a21 = m1[1 + 2 * 3], a22 = m1[2 + 2 * 3];

    for (int i = 0; i < 3; i++) {
        m1[i + 0 * 3] = a00 * m2[i + 0 * 3] + a01 * m2[i + 1 * 3] + a02 * m2[i + 2 * 3];
        m1[i + 1 * 3] = a10 * m2[i + 0 * 3] + a11 * m2[i + 1 * 3] + a12 * m2[i + 2 * 3];
        m1[i + 2 * 3] = a20 * m2[i + 0 * 3] + a21 * m2[i + 1 * 3] + a22 * m2[i + 2 * 3];
    }
}



static void Float3Multiply(const double in[3], const double mult[3 * 3], double out[3])
{
    for (size_t i = 0; i < 3; i++)
    {
        out[i] = mult[i + 0 * 3] * in[0] +
            mult[i + 1 * 3] * in[1] +
            mult[i + 2 * 3] * in[2];
    }
}

static void GetRGB2XYZMatrix(const struct cie1931_primaries* primaries,
    double out[3 * 3])
{
#define RED   0
#define GREEN 1
#define BLUE  2
    double X[3], Y[3], Z[3], S[3], W[3];
    double W_TO_S[3 * 3];

    X[RED] = primaries->red.x / primaries->red.y;
    X[GREEN] = 1;
    X[BLUE] = (1 - primaries->red.x - primaries->red.y) / primaries->red.y;

    Y[RED] = primaries->green.x / primaries->green.y;
    Y[GREEN] = 1;
    Y[BLUE] = (1 - primaries->green.x - primaries->green.y) / primaries->green.y;

    Z[RED] = primaries->blue.x / primaries->blue.y;
    Z[GREEN] = 1;
    Z[BLUE] = (1 - primaries->blue.x - primaries->blue.y) / primaries->blue.y;

    W_TO_S[0 + 0 * 3] = X[RED];
    W_TO_S[1 + 0 * 3] = X[GREEN];
    W_TO_S[2 + 0 * 3] = X[BLUE];
    W_TO_S[0 + 1 * 3] = Y[RED];
    W_TO_S[1 + 1 * 3] = Y[GREEN];
    W_TO_S[2 + 1 * 3] = Y[BLUE];
    W_TO_S[0 + 2 * 3] = Z[RED];
    W_TO_S[1 + 2 * 3] = Z[GREEN];
    W_TO_S[2 + 2 * 3] = Z[BLUE];

    Float3x3Inverse(W_TO_S);

    W[0] = primaries->white.x / primaries->white.y; /* Xw */
    W[1] = 1;                  /* Yw */
    W[2] = (1 - primaries->white.x - primaries->white.y) / primaries->white.y; /* Yw */

    Float3Multiply(W, W_TO_S, S);

    out[0 + 0 * 3] = S[RED] * X[RED];
    out[1 + 0 * 3] = S[GREEN] * Y[RED];
    out[2 + 0 * 3] = S[BLUE] * Z[RED];
    out[0 + 1 * 3] = S[RED] * X[GREEN];
    out[1 + 1 * 3] = S[GREEN] * Y[GREEN];
    out[2 + 1 * 3] = S[BLUE] * Z[GREEN];
    out[0 + 2 * 3] = S[RED] * X[BLUE];
    out[1 + 2 * 3] = S[GREEN] * Y[BLUE];
    out[2 + 2 * 3] = S[BLUE] * Z[BLUE];
#undef RED
#undef GREEN
#undef BLUE
}

/* from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */
static void GetXYZ2RGBMatrix(const struct cie1931_primaries* primaries,
    double out[3 * 3])
{
    GetRGB2XYZMatrix(primaries, out);
    Float3x3Inverse(out);
}

static void ChromaticAdaptation(const struct xy_primary* src_white,
    const struct xy_primary* dst_white,
    double in_out[3 * 3])
{
    if (fabs(src_white->x - dst_white->x) < 1e-6 &&
        fabs(src_white->y - dst_white->y) < 1e-6)
        return;

    /* TODO, see http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html */
}

static void GetPrimariesTransform(FLOAT Primaries[4 * 3], video_color_primaries_t src,
    video_color_primaries_t dst)
{
    //const struct cie1931_primaries* p_src = &STANDARD_PRIMARIES[src];
    //const struct cie1931_primaries* p_dst = &STANDARD_PRIMARIES[dst];
    const struct cie1931_primaries* p_src = COLOR_PRIMARIES_BT601_525_S;
    const struct cie1931_primaries* p_dst = COLOR_PRIMARIES_BT601_525_S;
    double rgb2xyz[3 * 3], xyz2rgb[3 * 3];

    /* src[RGB] -> src[XYZ] */
    GetRGB2XYZMatrix(p_src, rgb2xyz);

    /* src[XYZ] -> dst[XYZ] */
    ChromaticAdaptation(&p_src->white, &p_dst->white, rgb2xyz);

    /* dst[XYZ] -> dst[RGB] */
    GetXYZ2RGBMatrix(p_dst, xyz2rgb);

    /* src[RGB] -> src[XYZ] -> dst[XYZ] -> dst[RGB] */
    Float3x3Multiply(xyz2rgb, rgb2xyz);

    for (size_t i = 0; i < 3; ++i)
    {
        for (size_t j = 0; j < 3; ++j)
            Primaries[j + i * 4] = xyz2rgb[j + i * 3];
        Primaries[3 + i * 4] = 0;
    }
}

static void MultMat43(FLOAT dst[4 * 3], const FLOAT left[4 * 3], const FLOAT right[4 * 3])
{
    // Cache the invariants in registers
    FLOAT x = left[0 * 4 + 0];
    FLOAT y = left[0 * 4 + 1];
    FLOAT z = left[0 * 4 + 2];
    FLOAT w = left[0 * 4 + 3];
    // Perform the operation on the first row
    dst[0 * 4 + 0] = (right[0 * 4 + 0] * x) + (right[1 * 4 + 0] * y) + (right[2 * 4 + 0] * z) + (right[3 * 4 + 0] * w);
    dst[0 * 4 + 1] = (right[0 * 4 + 1] * x) + (right[1 * 4 + 1] * y) + (right[2 * 4 + 1] * z) + (right[3 * 4 + 1] * w);
    dst[0 * 4 + 2] = (right[0 * 4 + 2] * x) + (right[1 * 4 + 2] * y) + (right[2 * 4 + 2] * z) + (right[3 * 4 + 2] * w);
    dst[0 * 4 + 3] = (right[0 * 4 + 3] * x) + (right[1 * 4 + 3] * y) + (right[2 * 4 + 3] * z) + (right[3 * 4 + 3] * w);
    // Repeat for all the other rows
    x = left[1 * 4 + 0];
    y = left[1 * 4 + 1];
    z = left[1 * 4 + 2];
    w = left[1 * 4 + 3];
    dst[1 * 4 + 0] = (right[0 * 4 + 0] * x) + (right[1 * 4 + 0] * y) + (right[2 * 4 + 0] * z) + (right[3 * 4 + 0] * w);
    dst[1 * 4 + 1] = (right[0 * 4 + 1] * x) + (right[1 * 4 + 1] * y) + (right[2 * 4 + 1] * z) + (right[3 * 4 + 1] * w);
    dst[1 * 4 + 2] = (right[0 * 4 + 2] * x) + (right[1 * 4 + 2] * y) + (right[2 * 4 + 2] * z) + (right[3 * 4 + 2] * w);
    dst[1 * 4 + 3] = (right[0 * 4 + 3] * x) + (right[1 * 4 + 3] * y) + (right[2 * 4 + 3] * z) + (right[3 * 4 + 3] * w);
    x = left[2 * 4 + 0];
    y = left[2 * 4 + 1];
    z = left[2 * 4 + 2];
    w = left[2 * 4 + 3];
    dst[2 * 4 + 0] = (right[0 * 4 + 0] * x) + (right[1 * 4 + 0] * y) + (right[2 * 4 + 0] * z) + (right[3 * 4 + 0] * w);
    dst[2 * 4 + 1] = (right[0 * 4 + 1] * x) + (right[1 * 4 + 1] * y) + (right[2 * 4 + 1] * z) + (right[3 * 4 + 1] * w);
    dst[2 * 4 + 2] = (right[0 * 4 + 2] * x) + (right[1 * 4 + 2] * y) + (right[2 * 4 + 2] * z) + (right[3 * 4 + 2] * w);
    dst[2 * 4 + 3] = (right[0 * 4 + 3] * x) + (right[1 * 4 + 3] * y) + (right[2 * 4 + 3] * z) + (right[3 * 4 + 3] * w);
    // x = left[3*4 + 0];
    // y = left[3*4 + 1];
    // z = left[3*4 + 2];
    // w = left[3*4 + 3];
    // dst[3*4 + 0] = (right[0*4 + 0] * x) + (right[1*4 + 0] * y) + (right[2*4 + 0] * z) + (right[3*4 + 0] * w);
    // dst[3*4 + 1] = (right[0*4 + 1] * x) + (right[1*4 + 1] * y) + (right[2*4 + 1] * z) + (right[3*4 + 1] * w);
    // dst[3*4 + 2] = (right[0*4 + 2] * x) + (right[1*4 + 2] * y) + (right[2*4 + 2] * z) + (right[3*4 + 2] * w);
    // dst[3*4 + 3] = (right[0*4 + 3] * x) + (right[1*4 + 3] * y) + (right[2*4 + 3] * z) + (right[3*4 + 3] * w);
}

int CD3D12Quad::SetupQuad(const video_format_t* fmt, const display_info_t* displayFormat)
{
    shaderConstants->LuminanceScale = (float)displayFormat->luminance_peak / GetFormatLuminance(fmt);

    /* pixel shader constant buffer */
    shaderConstants->Opacity = 1.0;
    if (fmt->i_visible_width == fmt->i_width)
        shaderConstants->BoundaryX = 1.0; /* let texture clamping happen */
    else
        shaderConstants->BoundaryX = (FLOAT)(fmt->i_visible_width - 1) / fmt->i_width;
    if (fmt->i_visible_height == fmt->i_height)
        shaderConstants->BoundaryY = 1.0; /* let texture clamping happen */
    else
        shaderConstants->BoundaryY = (FLOAT)(fmt->i_visible_height - 1) / fmt->i_height;

    const bool RGB_src_shader = DxgiIsRGBFormat(textureFormat);

    FLOAT itu_black_level = 0.f;
    FLOAT itu_achromacy = 0.f;
    if (!RGB_src_shader)
    {
        switch (textureFormat->bitsPerChannel)
        {
        case 8:
            /* Rec. ITU-R BT.709-6 ¶4.6 */
            itu_black_level = 16.f / 255.f;
            itu_achromacy = 128.f / 255.f;
            break;
        case 10:
            /* Rec. ITU-R BT.709-6 ¶4.6 */
            itu_black_level = 64.f / 1023.f;
            itu_achromacy = 512.f / 1023.f;
            break;
        case 12:
            /* Rec. ITU-R BT.2020-2 Table 5 */
            itu_black_level = 256.f / 4095.f;
            itu_achromacy = 2048.f / 4095.f;
            break;
        default:
            /* unknown bitdepth, use approximation for infinite bit depth */
            itu_black_level = 16.f / 256.f;
            itu_achromacy = 128.f / 256.f;
            break;
        }
    }

    static const FLOAT IDENTITY_4X3[4 * 3] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
    };

    /* matrices for studio range */
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion, in studio range */
    static const FLOAT COLORSPACE_BT601_YUV_TO_FULL_RGBA[4 * 3] = {
        1.164383561643836f,                 0.f,  1.596026785714286f, 0.f,
        1.164383561643836f, -0.391762290094914f, -0.812967647237771f, 0.f,
        1.164383561643836f,  2.017232142857142f,                 0.f, 0.f,
    };

    static const FLOAT COLORSPACE_FULL_RGBA_TO_BT601_YUV[4 * 3] = {
        0.299000f,  0.587000f,  0.114000f, 0.f,
       -0.168736f, -0.331264f,  0.500000f, 0.f,
        0.500000f, -0.418688f, -0.081312f, 0.f,
    };

    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion, in studio range */
    static const FLOAT COLORSPACE_BT709_YUV_TO_FULL_RGBA[4 * 3] = {
        1.164383561643836f,                 0.f,  1.792741071428571f, 0.f,
        1.164383561643836f, -0.213248614273730f, -0.532909328559444f, 0.f,
        1.164383561643836f,  2.112401785714286f,                 0.f, 0.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion, in studio range */
    static const FLOAT COLORSPACE_BT2020_YUV_TO_FULL_RGBA[4 * 3] = {
        1.164383561643836f,  0.000000000000f,  1.678674107143f, 0.f,
        1.164383561643836f, -0.127007098661f, -0.440987687946f, 0.f,
        1.164383561643836f,  2.141772321429f,  0.000000000000f, 0.f,
    };

    FLOAT WhitePoint[4 * 3];
    memcpy(WhitePoint, IDENTITY_4X3, sizeof(WhitePoint));

    const FLOAT* ppColorspace;
    if (RGB_src_shader == DxgiIsRGBFormat(displayFormat->pixelFormat))
    {
        ppColorspace = IDENTITY_4X3;
    }
    else if (RGB_src_shader)
    {
        ppColorspace = COLORSPACE_FULL_RGBA_TO_BT601_YUV;
        WhitePoint[0 * 4 + 3] = -itu_black_level;
        WhitePoint[1 * 4 + 3] = itu_achromacy;
        WhitePoint[2 * 4 + 3] = itu_achromacy;
    }
    else
    {
        switch (fmt->space) {
        case COLOR_SPACE_BT709:
            ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
            break;
        case COLOR_SPACE_BT2020:
            ppColorspace = COLORSPACE_BT2020_YUV_TO_FULL_RGBA;
            break;
        case COLOR_SPACE_BT601:
            ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
            break;
        default:
        case COLOR_SPACE_UNDEF:
            if (fmt->i_height > 576)
            {
                ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
            }
            else
            {
                ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
            }
            break;
        }

        /* all matrices work in studio range and output in full range */
        WhitePoint[0 * 4 + 3] = -itu_black_level;
        WhitePoint[1 * 4 + 3] = -itu_achromacy;
        WhitePoint[2 * 4 + 3] = -itu_achromacy;
    }

    MultMat43(shaderConstants->Colorspace, ppColorspace, WhitePoint);

    if (fmt->primaries != displayFormat->primaries)
    {
        FLOAT Primaries[4 * 3];
        GetPrimariesTransform(Primaries, fmt->primaries, displayFormat->primaries);
        MultMat43(shaderConstants->Colorspace, Primaries, shaderConstants->Colorspace);
    }

    for (size_t i = 0; i < DXGI_MAX_SHADER_VIEW; i++)
    {
        cropViewport[i].MinDepth = 0.0f;
        cropViewport[i].MaxDepth = 1.0f;
    }

    return 0;
}

void CD3D12Quad::UpdateViewport(const RECT* rect, const d3d_format_t* display)
{
    LONG srcAreaWidth, srcAreaHeight;

    srcAreaWidth = RECTWidth(*rect);
    srcAreaHeight = RECTHeight(*rect);

    cropViewport[0].TopLeftX = rect->left;
    cropViewport[0].TopLeftY = rect->top;
    cropViewport[0].Width = srcAreaWidth;
    cropViewport[0].Height = srcAreaHeight;

    m_scissorRect[0] = *rect;

    switch (textureFormat->formatTexture)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        cropViewport[1].TopLeftX = rect->left / 2;
        cropViewport[1].TopLeftY = rect->top / 2;
        cropViewport[1].Width = srcAreaWidth / 2;
        cropViewport[1].Height = srcAreaHeight / 2;

        m_scissorRect[1].left = rect->left / 2;
        m_scissorRect[1].top = rect->top / 2;
        m_scissorRect[1].right = rect->left + srcAreaWidth / 2;
        m_scissorRect[1].top = rect->top + srcAreaHeight / 2;

        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y410:
        if (display->formatTexture == DXGI_FORMAT_NV12 ||
            display->formatTexture == DXGI_FORMAT_P010)
        {
            cropViewport[1].TopLeftX = rect->left / 2;
            cropViewport[1].TopLeftY = rect->top / 2;
            cropViewport[1].Width = srcAreaWidth / 2;
            cropViewport[1].Height = srcAreaHeight / 2;
        }
        break;
    case DXGI_FORMAT_UNKNOWN:
        switch (textureFormat->fourcc)
        {
        case MAKEFOURCC('Y','U','V','A')://:VLC_CODEC_YUVA:
            if (display->formatTexture != DXGI_FORMAT_NV12 &&
                display->formatTexture != DXGI_FORMAT_P010)
            {
                cropViewport[1] = cropViewport[2] =
                    cropViewport[3] = cropViewport[0];

                m_scissorRect[1] = m_scissorRect[2] =
                    m_scissorRect[3] = m_scissorRect[0];
                break;
            }
            /* fallthrough */
        case MAKEFOURCC('I', '0', 'A', 'L')://VLC_CODEC_I420_10L:
        case MAKEFOURCC('I', '4', '2', '0')://VLC_CODEC_I420:
            cropViewport[1].TopLeftX = rect->left / 2;
            cropViewport[1].TopLeftY = rect->top / 2;
            cropViewport[1].Width = srcAreaWidth / 2;
            cropViewport[1].Height = srcAreaHeight / 2;
            cropViewport[2] = cropViewport[1];

            m_scissorRect[1].left = rect->left / 2;
            m_scissorRect[1].top = rect->top / 2;
            m_scissorRect[1].right = rect->left + srcAreaWidth / 2;
            m_scissorRect[1].top = rect->top + srcAreaHeight / 2;
            m_scissorRect[2] = m_scissorRect[1];
            break;
        }
        break;
    default:
        assert(0);
    }
}

void CD3D12Quad::ReleasePipeline(d3d12_pipeline_t* pipeline)
{
    if (pipeline->m_rootSignature)
    {
        pipeline->m_rootSignature->Release();
        pipeline->m_rootSignature = NULL;
    }
    if (pipeline->m_pipelineState)
    {
        pipeline->m_pipelineState->Release();
        pipeline->m_pipelineState = NULL;
    }
}

void CD3D12Quad::ReleasePixelShader()
{
    (&m_cbvSrvHeap)->heap->Release();
    
    for (size_t i = 0; i < ARRAY_SIZE(m_pipelines); i++)
    {
        ReleasePipeline(&m_pipelines[i]);
        if ((&commands[i])->m_commandAllocator)
        {
            (&commands[i])->m_commandAllocator->Release();
            
            (&commands[i])->m_commandAllocator = NULL;
        }
        if ((&commands[i])->m_commandList)
        {
            (&commands[i])->m_commandList->Release();
            
            (&commands[i])->m_commandList = NULL;
        }
        
    }
}
