#include "CKDX12Rasterizer.h"
#include "CKDX12RasterizerCommon.h"

CKDX12Buffer::CKDX12Buffer(D3D12MA::Allocator *allocator, bool cpuAccessible, size_t size) :
    m_cpuAccessible(cpuAccessible)
{
    HRESULT hr;
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = m_cpuAccessible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
    D3DCall(
        allocator->CreateResource(&allocationDesc, &resourceDesc,
                                  cpuAccessible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_SOURCE,
                                  nullptr, &Allocation, IID_PPV_ARGS(&Resource)));
    GPUAddress = Resource->GetGPUVirtualAddress();
    if (cpuAccessible)
    {
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
        D3DCall(Resource->Map(0, &readRange, &CPUAddress));
    }
}