#ifndef CKDX12RINGBUFFER_H
#define CKDX12RINGBUFFER_H

#include "CKDX12RingBufferBase.h"

#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"

struct CKDX12AllocatedResource
{
    CKDX12AllocatedResource(Microsoft::WRL::ComPtr<ID3D12Resource> pBuff, size_t ThisOffset, size_t ThisSize) :
        pBuffer(pBuff), Offset(ThisOffset), Size(ThisSize)
    {
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer = nullptr;
    size_t Offset = 0;
    size_t Size = 0;
    void *CPUAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
};

class CKDX12RingBuffer : public CKDX12RingBufferBase
{
    
public:
    CKDX12RingBuffer(size_t MaxSize, Microsoft::WRL::ComPtr<ID3D12Device> pDevice, bool AllowCPUAccess);

    CKDX12RingBuffer(CKDX12RingBuffer &&rhs) = default;
    CKDX12RingBuffer &operator=(CKDX12RingBuffer &&rhs) noexcept = default;
    CKDX12RingBuffer(const CKDX12RingBuffer &) = delete;
    CKDX12RingBuffer &operator=(CKDX12RingBuffer &) = delete;
    ~CKDX12RingBuffer() = default;

    CKDX12AllocatedResource Allocate(size_t SizeInBytes);
private:
    void *m_CpuVirtualAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pBuffer;
};

#endif // CKDX12RINGBUFFER_H
