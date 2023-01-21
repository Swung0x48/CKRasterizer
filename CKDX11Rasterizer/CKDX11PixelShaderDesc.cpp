#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11PixelShaderDesc::Create(CKDX11RasterizerContext *ctx)
{
    HRESULT hr;
    D3DCall(D3DCompile(m_Function, m_FunctionSize, nullptr, nullptr, nullptr, DxEntryPoint, DxTarget, 0, 0,
                       DxBlob.GetAddressOf(), DxErrorMsgs.GetAddressOf()));
    if (FAILED(hr) && DxErrorMsgs)
    {
        const char *errorMsg = (const char *)DxErrorMsgs->GetBufferPointer();
        MessageBox(nullptr, errorMsg, TEXT("Pixel Shader Compilation Error"), MB_RETRYCANCEL);
    }
    D3DCall(ctx->m_Device->CreatePixelShader(DxBlob->GetBufferPointer(), DxBlob->GetBufferSize(), nullptr, &DxShader));
    
    return SUCCEEDED(hr);
}

void CKDX11PixelShaderDesc::Bind(CKDX11RasterizerContext *ctx) {
    ctx->m_DeviceContext->PSSetShader(DxShader.Get(), nullptr, 0);
}
