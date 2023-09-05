#ifndef CKDX12DESCIPTORRING_H
#define CKDX12DESCIPTORRING_H

#include "CKDX12RingBufferBase.h"
#include "CKDX12RingBuffer.h"
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"

class CKDX12DescriptorRing: public CKDX12RingBufferBase
{
public:
    CKDX12DescriptorRing(size_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, Microsoft::WRL::ComPtr<ID3D12Device> device);
    CKDX12DescriptorRing(CKDX12DescriptorRing &&) = default;
    CKDX12DescriptorRing(CKDX12DescriptorRing &) = delete;
    CKDX12DescriptorRing& operator=(CKDX12DescriptorRing&) = delete;
    CKDX12DescriptorRing &operator=(CKDX12DescriptorRing &&that) = default;
    ~CKDX12DescriptorRing() = default;

    HRESULT CreateConstantBufferView(const CKDX12AllocatedResource &resource, CD3DX12_GPU_DESCRIPTOR_HANDLE &gpuHandle);
    HRESULT CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                     CD3DX12_GPU_DESCRIPTOR_HANDLE &gpuHandle);

public:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_Heap;
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
    UINT m_IncrementSize = 0;
};

#endif // CKDX12DESCIPTORRING_H