#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKDX11TextureDesc::CKDX11TextureDesc(CKTextureDesc* desc): CKTextureDesc(*desc) {
	DxDesc.Width = desc->Format.Width;
    DxDesc.Height = desc->Format.Height;
    DxDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DxDesc.MipLevels = desc->MipMapCount;
    DxDesc.SampleDesc.Count = 1;
    DxDesc.Usage = D3D11_USAGE_IMMUTABLE;
    DxDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    DxDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    DxDesc.MiscFlags = 0;
}

CKBOOL CKDX11TextureDesc::Create(CKDX11RasterizerContext *ctx, void* data)
{
    HRESULT hr;
    D3D11_SUBRESOURCE_DATA resData = {};
    resData.pSysMem = data;
    resData.SysMemPitch = Format.Width * 4;
    D3DCall(ctx->m_Device->CreateTexture2D(&DxDesc, &resData, DxTexture.GetAddressOf()));
    D3DCall(ctx->m_Device->CreateShaderResourceView(DxTexture.Get(), nullptr, DxSRV.GetAddressOf()));
    return SUCCEEDED(hr);
}

void CKDX11TextureDesc::Bind(CKDX11RasterizerContext *ctx) {
    ctx->m_DeviceContext->PSSetShaderResources(0, 1, DxSRV.GetAddressOf());
}

void CKDX11TextureDesc::Load(CKDX11RasterizerContext *ctx, void *data) {}
