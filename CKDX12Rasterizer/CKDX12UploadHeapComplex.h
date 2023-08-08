#ifndef CKDX12UPLOADHEAPCOMPLEX_H
#define CKDX12UPLOADHEAPCOMPLEX_H

#include "CKDX12DynamicUploadHeap.h"
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class CKDX12UploadHeapComplex
{
public:
    CKDX12UploadHeapComplex(Microsoft::WRL::ComPtr<ID3D12Device> device, size_t size, bool noShrink = false) :
        m_Device(device), m_noShrink(noShrink),
        m_StagingHeap(true, device, size, noShrink),
        m_DefaultHeap(false, device, size, noShrink)
    {
    }

    CKDX12UploadHeapComplex(const CKDX12UploadHeapComplex &) = delete;
    CKDX12UploadHeapComplex(CKDX12UploadHeapComplex &&) = delete;
    CKDX12UploadHeapComplex &operator=(const CKDX12UploadHeapComplex &) = delete;
    CKDX12UploadHeapComplex &operator=(CKDX12UploadHeapComplex &&) = delete;

    CKDX12AllocatedResource Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

    void FinishFrame(UINT64 nextFenceValue, UINT64 lastCompletedFenceValue);

private:
    static constexpr size_t DEFAULT_ALIGN = 1;
    const bool m_noShrink = false;
    CKDX12DynamicUploadHeap m_StagingHeap;
    CKDX12DynamicUploadHeap m_DefaultHeap;
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
};

#endif // CKDX12UPLOADHEAPCOMPLEX_H