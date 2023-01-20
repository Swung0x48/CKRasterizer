#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11PixelShaderDesc::Create(CKDX11RasterizerContext *ctx)
{
    HRESULT hr;
    D3DCall(D3DCompile(m_Function, m_FunctionSize, nullptr, nullptr, nullptr, DxEntryPoint, DxTarget, 0, 0,
                       DxBlob.GetAddressOf(), DxErrorMsgs.GetAddressOf()));
    D3DCall(ctx->m_Device->CreatePixelShader(DxBlob->GetBufferPointer(), DxBlob->GetBufferSize(), nullptr, &DxShader));
    return SUCCEEDED(hr);
}