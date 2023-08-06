//#include "CKDX12Rasterizer.h"
//#include "CKDX12RasterizerCommon.h"
//
//CKBOOL CKDX12IndexBufferDesc::Create(CKDX12RasterizerContext *ctx)
//{
//    HRESULT hr;
//    auto uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
//    auto ibResDesc = CD3DX12_RESOURCE_DESC::Buffer(m_MaxIndexCount * sizeof(CKWORD));
//    D3DCall(ctx->m_Device->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &ibResDesc,
//                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
//                                              IID_PPV_ARGS(&DxResource)));
//#if defined(DEBUG) || defined(_DEBUG)
//    DxResource->SetName(L"IB");
//#endif
//    DxView.BufferLocation = DxResource->GetGPUVirtualAddress();
//    DxView.Format = DXGI_FORMAT_R16_UINT;
//    DxView.SizeInBytes = m_MaxIndexCount * sizeof(CKWORD);
//    return SUCCEEDED(hr);
//}
//
//void *CKDX12IndexBufferDesc::Lock()
//{
//    HRESULT hr;
//    void *pData = nullptr;
//    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
//    D3DCall(DxResource->Map(0, &readRange, &pData));
//    return pData;
//}
//
//void CKDX12IndexBufferDesc::Unlock() { DxResource->Unmap(0, nullptr); }
