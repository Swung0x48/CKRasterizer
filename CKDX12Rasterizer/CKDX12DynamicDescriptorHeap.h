#ifndef CKDX12DYNAMICDESCIPTORHEAP_H
#define CKDX12DYNAMICDESCIPTORHEAP_H

#include "CKDX12DescriptorRing.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class CKDX12DynamicDescriptorHeap
{
public:
    CKDX12DynamicDescriptorHeap(size_t defaultSize, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                Microsoft::WRL::ComPtr<ID3D12Device> device) :
        m_Type(type), m_Device(device)
    {
        m_Heaps.emplace_back(defaultSize, type, device);
    }

    CKDX12DynamicDescriptorHeap(const CKDX12DynamicDescriptorHeap &) = delete;
    CKDX12DynamicDescriptorHeap(CKDX12DynamicDescriptorHeap &&) = default;
    CKDX12DynamicDescriptorHeap &operator=(const CKDX12DynamicDescriptorHeap &) = delete;
    CKDX12DynamicDescriptorHeap &operator=(CKDX12DynamicDescriptorHeap &&) = default;

    HRESULT CreateDescriptor(const CKDX12AllocatedResource &resource, CD3DX12_GPU_DESCRIPTOR_HANDLE &gpuHandle);
    void FinishFrame(UINT64 nextFenceValue, UINT64 lastCompletedFenceValue);

public:
    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc = {};
    std::vector<CKDX12DescriptorRing> m_Heaps;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    UINT m_IncrementSize = 0;
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
};

#endif // CKDX12DYNAMICDESCIPTORHEAP_H