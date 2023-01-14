#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKDX11RasterizerDriver::CKDX11RasterizerDriver(CKDX11Rasterizer *rst) {
    m_Owner = rst;
}

CKDX11RasterizerDriver::~CKDX11RasterizerDriver() {

}

CKRasterizerContext *CKDX11RasterizerDriver::CreateContext() {
    auto* ctx = new CKDX11RasterizerContext();
    ctx->m_Driver = this;
    ctx->m_Owner = static_cast<CKDX11Rasterizer *>(m_Owner);
    m_Contexts.PushBack(ctx);
    return ctx;
}

CKBOOL CKDX11RasterizerDriver::InitializeCaps(UINT AdapterIndex, IDXGIAdapter1* Adapter) {
    HRESULT hr;

    m_AdapterIndex = AdapterIndex;
    m_Adapter = Adapter;
    D3DCall(m_Adapter->GetDesc1(&m_DXGIAdapterDesc));
    return TRUE;
}

CKBOOL CKDX11RasterizerDriver::IsTextureFormatOk(DXGI_FORMAT TextureFormat, DXGI_FORMAT AdapterFormat, DWORD Usage) {
    return FALSE;
}

DXGI_FORMAT CKDX11RasterizerDriver::FindNearestTextureFormat(CKTextureDesc *desc, DXGI_FORMAT AdapterFormat, DWORD Usage) {
    return DXGI_FORMAT_UNKNOWN;
}
DXGI_FORMAT CKDX11RasterizerDriver::FindNearestRenderTargetFormat(int Bpp, CKBOOL Windowed) {
    return DXGI_FORMAT_UNKNOWN;

}
DXGI_FORMAT CKDX11RasterizerDriver::FindNearestDepthFormat(DXGI_FORMAT pf, int ZBpp, int StencilBpp) {
    return DXGI_FORMAT_UNKNOWN;

}

CKBOOL CKDX11RasterizerDriver::CheckDeviceFormat(DXGI_FORMAT AdapterFormat, DXGI_FORMAT CheckFormat) {
    return FALSE;

}

BOOL CKDX11RasterizerDriver::CheckDepthStencilMatch(DXGI_FORMAT AdapterFormat, DXGI_FORMAT CheckFormat) {
    return FALSE;
}