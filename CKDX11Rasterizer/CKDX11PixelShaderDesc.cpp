#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11PixelShaderDesc::Compile(CKDX11RasterizerContext *ctx)
{
    HRESULT hr;
    UINT flag1 = 0;
#if defined(DEBUG) || defined(_DEBUG)
    flag1 |= D3DCOMPILE_DEBUG;
#endif
    D3DCall(D3DCompile(m_Function, m_FunctionSize, nullptr, nullptr, nullptr, DxEntryPoint, DxTarget, flag1, 0,
                       DxBlob.GetAddressOf(), DxErrorMsgs.GetAddressOf()));
    if (FAILED(hr) && DxErrorMsgs)
    {
        const char *errorMsg = (const char *)DxErrorMsgs->GetBufferPointer();
        MessageBox(nullptr, errorMsg, TEXT("Pixel Shader Compilation Error"), MB_RETRYCANCEL);
    }
    return Create(ctx);
}

CKBOOL CKDX11PixelShaderDesc::Create(CKDX11RasterizerContext *ctx)
{
    HRESULT hr;
    // D3DCall(ctx->m_Device->CreatePixelShader(DxBlob->GetBufferPointer(), DxBlob->GetBufferSize(), nullptr, &DxShader));
    D3DCall(ctx->m_Device->CreatePixelShader(m_Function, m_FunctionSize, nullptr, DxShader.ReleaseAndGetAddressOf()));
    return SUCCEEDED(hr);
}

void CKDX11PixelShaderDesc::Bind(CKDX11RasterizerContext *ctx) {
    ctx->m_DeviceContext->PSSetShader(DxShader.Get(), nullptr, 0);
}
