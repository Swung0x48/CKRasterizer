#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11ConstantBufferDesc::Create(CKDX11RasterizerContext *ctx, UINT size)
{
    HRESULT hr;

    DxDesc.Usage = D3D11_USAGE_DYNAMIC;
    DxDesc.ByteWidth = size;
    DxDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    DxDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3DCall(ctx->m_Device->CreateBuffer(&DxDesc, nullptr, DxBuffer.ReleaseAndGetAddressOf()));
    return SUCCEEDED(hr);
}
