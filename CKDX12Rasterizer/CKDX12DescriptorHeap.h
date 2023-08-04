#ifndef CKDX12DESCIPTORHEAP_H
#define CKDX12DESCIPTORHEAP_H

#include "CKDX12RingBufferBase.h"
#include "CKDX12RingBuffer.h"
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"

class CKDX12DescriptorHeap: public CKDX12RingBufferBase
{
public:
    CKDX12DescriptorHeap(size_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, Microsoft::WRL::ComPtr<ID3D12Device> device);
    CKDX12DescriptorHeap(CKDX12DescriptorHeap &) = delete;
    CKDX12DescriptorHeap& operator=(CKDX12DescriptorHeap) = delete;

    HRESULT CreateView(CKDX12AllocatedResource &resource, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle);
private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc = {};
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
    UINT m_IncrementSize = 0;
};

#endif // CKDX12DESCIPTORHEAP_H