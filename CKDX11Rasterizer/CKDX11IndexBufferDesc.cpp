#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11IndexBufferDesc::Create(CKDX11RasterizerContext *ctx)
{
    HRESULT hr;
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    D3D11_CPU_ACCESS_FLAG flag = D3D11_CPU_ACCESS_WRITE;
    if (m_Flags & CKRST_VB_DYNAMIC)
        usage = D3D11_USAGE_DYNAMIC;
    if (m_Flags & CKRST_VB_WRITEONLY)
        flag = D3D11_CPU_ACCESS_WRITE;

    DxDesc.Usage = usage;
    DxDesc.ByteWidth = m_MaxIndexCount * sizeof(CKWORD);
    DxDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    DxDesc.CPUAccessFlags = flag;

    D3DCall(ctx->m_Device->CreateBuffer(&DxDesc, nullptr, DxBuffer.GetAddressOf()));
    return SUCCEEDED(hr);
}

void *CKDX11IndexBufferDesc::Lock(CKDX11RasterizerContext *ctx, CKDWORD offset, CKDWORD len, bool overwrite) {
    D3D11_MAP mapType = overwrite ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
    
    assert(offset + len <= sizeof(CKWORD) * m_MaxIndexCount);
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE ms;
    D3DCall(ctx->m_DeviceContext->Map(DxBuffer.Get(), NULL, mapType, NULL, &ms));
    if (SUCCEEDED(hr))
        // seems like d3d11 does not give us an option to map a portion of data...
        return (char *)ms.pData + offset;
    return nullptr;
}

void CKDX11IndexBufferDesc::Unlock(CKDX11RasterizerContext *ctx)
{
    ctx->m_DeviceContext->Unmap(DxBuffer.Get(), NULL);
}
