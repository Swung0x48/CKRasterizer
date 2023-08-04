#include "CKDX12RingBuffer.h"


CKDX12RingBuffer::CKDX12RingBuffer(size_t MaxSize, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, bool AllowCPUAccess) :
    CKDX12RingBufferBase(MaxSize), m_CpuVirtualAddress(nullptr), m_GpuVirtualAddress(0)
{
    HRESULT hr;
    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(MaxSize);

    D3D12_RESOURCE_STATES DefaultUsage;
    if (AllowCPUAccess)
    {
        HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
    else
    {
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    D3DCall(pDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc, DefaultUsage, nullptr,
                                     IID_PPV_ARGS(&m_pBuffer)));

#if defined DEBUG || _DEBUG
    if (AllowCPUAccess)
    {
        D3DCall(m_pBuffer->SetName(L"Upload Ring Buffer"));
    } else
    {
        D3DCall(m_pBuffer->SetName(L"Default Ring Buffer"));
    }
#endif

    ResourceDesc.Width = MaxSize;

    m_GpuVirtualAddress = m_pBuffer->GetGPUVirtualAddress();

    if (AllowCPUAccess)
    {
        D3DCall(m_pBuffer->Map(0, nullptr, &m_CpuVirtualAddress));
    }
}

CKDX12AllocatedResource CKDX12RingBuffer::Allocate(size_t SizeInBytes)
{
    auto Offset = CKDX12RingBufferBase::Allocate(SizeInBytes);
    if (Offset == CKDX12RingBufferBase::InvalidOffset)
        return {nullptr, 0, 0};
    
    CKDX12AllocatedResource alloc(m_pBuffer, Offset, SizeInBytes);
    alloc.GPUAddress = m_GpuVirtualAddress + Offset;
    if (m_CpuVirtualAddress)
        alloc.CPUAddress = reinterpret_cast<char *>(m_CpuVirtualAddress) + Offset;
    return alloc;
}