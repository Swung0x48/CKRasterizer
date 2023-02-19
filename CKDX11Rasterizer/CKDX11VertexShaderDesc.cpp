#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKBOOL CKDX11VertexShaderDesc::Compile(CKDX11RasterizerContext *ctx)
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
        MessageBox(nullptr, errorMsg, TEXT("Vertex Shader Compilation Error"), MB_RETRYCANCEL);
    }
    return Create(ctx);
}

CKBOOL CKDX11VertexShaderDesc::Create(CKDX11RasterizerContext* ctx)
{
    HRESULT hr;
    bool succeeded = FVF::CreateInputLayoutFromFVF(DxFVF, DxInputElementDesc);
    if (!succeeded)
        return FALSE;
    // if (DxBlob->GetBufferPointer() != nullptr)
    // {
    //     D3DCall(ctx->m_Device->CreateInputLayout(DxInputElementDesc.data(), DxInputElementDesc.size(),
    //                                              DxBlob->GetBufferPointer(), DxBlob->GetBufferSize(),
    //                                              DxInputLayout.GetAddressOf()));
    //     D3DCall(ctx->m_Device->CreateVertexShader(DxBlob->GetBufferPointer(), DxBlob->GetBufferSize(), nullptr, &DxShader));
    // } else
    // {
        // D3DCall(ctx->m_Device->CreateInputLayout(DxInputElementDesc.data(), DxInputElementDesc.size(),
        //                                          m_Function, m_FunctionSize,
        //                                          DxInputLayout.GetAddressOf()));
        D3DCall(ctx->m_Device->CreateVertexShader(m_Function, m_FunctionSize, nullptr, &DxShader));
    // }
    return SUCCEEDED(hr);
}

void CKDX11VertexShaderDesc::Bind(CKDX11RasterizerContext *ctx) {
    // ctx->m_DeviceContext->IASetInputLayout(DxInputLayout.Get());
    ctx->m_DeviceContext->VSSetShader(DxShader.Get(), nullptr, 0);
}
