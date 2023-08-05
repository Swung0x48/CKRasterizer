#include "CKDX12DynamicDescriptorHeap.h"

HRESULT CKDX12DynamicDescriptorHeap::CreateDescriptor(const CKDX12AllocatedResource &resource,
                                                      CD3DX12_GPU_DESCRIPTOR_HANDLE &gpuHandle)
{
    HRESULT hr = m_Heaps.back().CreateDescriptor(resource, gpuHandle);
    if (hr == E_OUTOFMEMORY)
    {
        // Create new buffer
        auto newSize = m_Heaps.back().GetMaxSize() * 2;
        // Make sure the buffer is large enough for the requested chunk
        while (newSize < m_Heaps.back().GetUsedSize() + 1)
            newSize *= 2;
        m_Heaps.emplace_back(newSize, m_Type, m_Device);
        hr = m_Heaps.back().CreateDescriptor(resource, gpuHandle);
    } else
    {
        D3DCall(hr);
    }
    return hr;
}

void CKDX12DynamicDescriptorHeap::FinishFrame(UINT64 FenceValue, UINT64 LastCompletedFenceValue)
{
    size_t bufToDelete = 0;
    for (size_t i = 0; i < m_Heaps.size(); ++i)
    {
        auto &heap = m_Heaps[i];
        heap.FinishCurrentFrame(FenceValue);
        heap.ReleaseCompletedFrames(LastCompletedFenceValue);
        if (bufToDelete == i && i < m_Heaps.size() - 1 && heap.IsEmpty())
        {
            ++bufToDelete;
        }
    }

    if (bufToDelete > 0)
        m_Heaps.erase(m_Heaps.begin(), m_Heaps.begin() + bufToDelete);
}
