#include "CKDX12DescriptorRing.h"

CKDX12DescriptorRing::CKDX12DescriptorRing(size_t size, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                           Microsoft::WRL::ComPtr<ID3D12Device> device):
    CKDX12RingBufferBase(size),
    m_Device(device),
    m_IncrementSize(m_Device->GetDescriptorHandleIncrementSize(type))
{
    HRESULT hr;
    // currently only supporting cbv
    assert(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_HeapDesc.NumDescriptors = size;
    m_HeapDesc.Type = type;
    m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_HeapDesc.NodeMask = 0; // We're not supporting multi-GPU
    D3DCall(device->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(&m_Heap)));
    /*m_HeadHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_Heap->GetCPUDescriptorHandleForHeapStart());
    m_TailHandle = m_HeadHandle;*/
}

HRESULT CKDX12DescriptorRing::CreateConstantBufferView(const CKDX12AllocatedResource &resource, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle)
{
    auto offset = CKDX12RingBufferBase::Allocate(1);
    if (offset == InvalidOffset)
        return E_OUTOFMEMORY;
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
    desc.BufferLocation = resource.GPUAddress;
    desc.SizeInBytes = resource.Size;
    assert(m_HeapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_Heap->GetCPUDescriptorHandleForHeapStart(), offset, m_IncrementSize);
    m_Device->CreateConstantBufferView(&desc, cpuHandle);
    gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), offset, m_IncrementSize);
    return S_OK;
}

HRESULT CKDX12DescriptorRing::CreateShaderResourceView(ID3D12Resource *pResource,
                                                       const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                       CD3DX12_GPU_DESCRIPTOR_HANDLE &gpuHandle)
{
    HRESULT hr = S_OK;
    auto offset = CKDX12RingBufferBase::Allocate(1);
    if (offset == InvalidOffset)
        return E_OUTOFMEMORY;
    auto cpuHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m_Heap->GetCPUDescriptorHandleForHeapStart(), offset, m_IncrementSize);
    m_Device->CreateShaderResourceView(pResource, pDesc, cpuHandle);
    gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetGPUDescriptorHandleForHeapStart(), offset, m_IncrementSize);
    return hr;
}
