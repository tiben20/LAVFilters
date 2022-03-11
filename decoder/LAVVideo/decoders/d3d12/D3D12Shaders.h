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


#include "D3D12Format.h"
#include "d3d12.h"

D3D12_CPU_DESCRIPTOR_HANDLE D3D12_HeapCPUHandle(d3d12_heap_t*, size_t sub_resource);
D3D12_GPU_DESCRIPTOR_HANDLE D3D12_HeapGPUHandle(d3d12_heap_t*, size_t sub_resource);


typedef HRESULT(WINAPI* pD3DCompile)
(LPCVOID                         pSrcData,
    SIZE_T                          SrcDataSize,
    LPCSTR                          pFileName,
    CONST D3D_SHADER_MACRO* pDefines,
    ID3DInclude* pInclude,
    LPCSTR                          pEntrypoint,
    LPCSTR                          pTarget,
    UINT                            Flags1,
    UINT                            Flags2,
    ID3DBlob** ppCode,
    ID3DBlob** ppErrorMsgs);
typedef struct
{
    HINSTANCE                 compiler_dll; /* handle of the opened d3dcompiler dll */
    union {
        pD3DCompile               OurD3DCompile;
    };
} d3d_shader_compiler_t;

static inline void D3D_ShaderBlobRelease(d3d_shader_blob* blob)
{
    if (blob->pf_release)
        blob->pf_release(blob);
    //*blob = (d3d_shader_blob){ 0 };
    blob = nullptr;
    //ZeroMemory(&blob, sizeof(d3d_shader_blob));
}

static HRESULT D3D12_HeapInit(ID3D12Device* d3d_dev, d3d12_heap_t* heap,
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

static D3D12_CPU_DESCRIPTOR_HANDLE D3D12_HeapCPUHandle(d3d12_heap_t* heap, size_t sub_resource)
{
    union u
    {
        D3D12_CPU_DESCRIPTOR_HANDLE h;
        uintptr_t p;
    } t;
    t.h = heap->heap->GetCPUDescriptorHandleForHeapStart();
    t.p += sub_resource * heap->m_descriptorSize;
    return t.h;
}

static D3D12_GPU_DESCRIPTOR_HANDLE D3D12_HeapGPUHandle(d3d12_heap_t* heap, size_t sub_resource)
{
    union u
    {
        D3D12_GPU_DESCRIPTOR_HANDLE h;
        uintptr_t p;
    } t;
    t.h = heap->heap->GetGPUDescriptorHandleForHeapStart();
    t.p += sub_resource * heap->m_descriptorSize;
    return t.h;
}

class CD3D12Shaders
{
public:
    CD3D12Shaders();
    ~CD3D12Shaders();

    HRESULT HeapInit(ID3D12Device* d3d_dev, d3d12_heap_t* heap,
        D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags,
        UINT NumDescriptors);
    void CD3D12Shaders::HeapRelease(d3d12_heap_t* heap);
    void CommandListRelease(d3d12_commands_t* cmd);
    HRESULT CompilePipelineState(const d3d_shader_compiler_t *compiler,
        ID3D12Device* d3d_dev,
        const d3d_format_t* textureFormat,
        const display_info_t* display, bool sharp,
        video_transfer_func_t transfer,
        bool src_full_range,
        video_projection_mode_t projection_mode,
        d3d12_commands_t commands[DXGI_MAX_RENDER_TARGET],
        d3d12_pipeline_t pipelines[DXGI_MAX_RENDER_TARGET],
        size_t shader_views[DXGI_MAX_RENDER_TARGET]);
    HRESULT FenceInit(d3d12_fence_t* fence, ID3D12Device* d3d_dev);
    void FenceRelease(d3d12_fence_t* fence);
    HRESULT CommandListCreateGraphic(ID3D12Device* d3d_dev, d3d12_commands_t* cmd);
    HRESULT CD3D12Shaders::CompileVertexShader(const d3d_shader_compiler_t *compiler,
        D3D_FEATURE_LEVEL feature_level, bool flat,
        d3d_shader_blob* blob);
private:

protected:

};
