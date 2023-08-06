#ifndef CKDX12DYNAMICUPLOADHEAP_H
#define CKDX12DYNAMICUPLOADHEAP_H

#include "CKDX12RingBuffer.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class CKDX12DynamicUploadHeap
{
public:
    CKDX12DynamicUploadHeap(bool IsCPUAccessible, Microsoft::WRL::ComPtr<ID3D12Device> device, size_t size) :
        m_IsCPUAccessible(IsCPUAccessible), m_Device(device)
    {
        CKDX12RingBuffer buffer(size, device, m_IsCPUAccessible);
        m_RingBuffers.emplace_back(std::move(buffer));
    }

    CKDX12DynamicUploadHeap(const CKDX12DynamicUploadHeap &) = delete;
    CKDX12DynamicUploadHeap(CKDX12DynamicUploadHeap &&) = delete;
    CKDX12DynamicUploadHeap &operator=(const CKDX12DynamicUploadHeap &) = delete;
    CKDX12DynamicUploadHeap &operator=(CKDX12DynamicUploadHeap &&) = delete;

    CKDX12AllocatedResource Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

    void FinishFrame(UINT64 nextFenceValue, UINT64 lastCompletedFenceValue);

private:
    static constexpr size_t DEFAULT_ALIGN = 1;
    const bool m_IsCPUAccessible = false;
    std::vector<CKDX12RingBuffer> m_RingBuffers;
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
};

#endif // CKDX12DYNAMICUPLOADHEAP_H