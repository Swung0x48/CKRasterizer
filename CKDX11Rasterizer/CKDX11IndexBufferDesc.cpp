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