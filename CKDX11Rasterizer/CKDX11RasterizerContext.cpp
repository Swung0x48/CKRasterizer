#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKDX11RasterizerContext::CKDX11RasterizerContext() {}
CKDX11RasterizerContext::~CKDX11RasterizerContext() {}
CKBOOL CKDX11RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
                                       CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
    HRESULT hr;

    SetWindowLongA((HWND)Window, GWL_STYLE, WS_OVERLAPPED | WS_SYSMENU);
    SetClassLongPtr((HWND)Window, GCLP_HBRBACKGROUND, (LONG)GetStockObject(NULL_BRUSH));

    DXGI_SWAP_CHAIN_DESC scd;
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

    scd.BufferCount = 2;
    scd.BufferDesc.Width = Width;
    scd.BufferDesc.Height = Height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // just use 32-bit color here, too lazy to check if valid
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = (HWND)Window;
    scd.SampleDesc.Count = 1; // ignore multisample for now
    scd.Windowed = !Fullscreen;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1};
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    auto *adapter = static_cast<CKDX11RasterizerDriver *>(m_Driver)->m_Adapter.Get();
    hr = D3D11CreateDeviceAndSwapChain(static_cast<CKDX11RasterizerDriver *>(m_Driver)->m_Adapter.Get(),
                                       D3D_DRIVER_TYPE_UNKNOWN, nullptr, creationFlags, featureLevels,
                                       _countof(featureLevels), D3D11_SDK_VERSION,
                                       &scd, &m_Swapchain, &m_Device, nullptr, &m_DeviceContext);
    if (hr == E_INVALIDARG)
    {
        D3DCall(D3D11CreateDeviceAndSwapChain(static_cast<CKDX11RasterizerDriver *>(m_Driver)->m_Adapter.Get(),
                                              D3D_DRIVER_TYPE_UNKNOWN, nullptr, creationFlags, &featureLevels[1],
                                              _countof(featureLevels) - 1,
                                              D3D11_SDK_VERSION, &scd, &m_Swapchain, &m_Device, nullptr,
                                              &m_DeviceContext));
    }
    else
    {
        D3DCall(hr);
    }

    ID3D11Texture2D *pBackBuffer = nullptr;
    D3DCall(m_Swapchain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)));
    D3DCall(m_Device->CreateRenderTargetView(pBackBuffer, nullptr, &m_BackBuffer));
    D3DCall(pBackBuffer->Release());
    m_DeviceContext->OMSetRenderTargets(1, &m_BackBuffer, NULL);

    m_Window = (HWND)Window;
    m_PosX = PosX;
    m_PosY = PosY;
    m_Fullscreen = Fullscreen;
    m_Bpp = Bpp;
    m_ZBpp = Zbpp;
    m_Width = Width;
    m_Height = Height;
    if (m_Fullscreen)
        m_Driver->m_Owner->m_FullscreenContext = this;

    return SUCCEEDED(hr);
}
CKBOOL CKDX11RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    return CKRasterizerContext::Resize(PosX, PosY, Width, Height, Flags);
}
CKBOOL CKDX11RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount,
                                      CKRECT *rects)
{
    if (!m_BackBuffer)
        return FALSE;
    m_DeviceContext->ClearRenderTargetView(m_BackBuffer.Get(), m_ClearColor);
    /* if (Flags & CKRST_CTXCLEAR_STENCIL)
        D3DCall(m_DeviceContext->ClearDepthStencilView());
    if (Flags & CKRST_)
    D3DCall(m_DeviceContext->ClearRenderTargetView())*/
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::BackToFront(CKBOOL vsync) {
    HRESULT hr;
    D3DCall(m_Swapchain->Present(vsync ? 1 : 0, 0));
    return SUCCEEDED(hr);
}

CKBOOL CKDX11RasterizerContext::BeginScene() { return CKRasterizerContext::BeginScene(); }
CKBOOL CKDX11RasterizerContext::EndScene() { return CKRasterizerContext::EndScene(); }
CKBOOL CKDX11RasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    return CKRasterizerContext::SetLight(Light, data);
}
CKBOOL CKDX11RasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    return CKRasterizerContext::EnableLight(Light, Enable);
}
CKBOOL CKDX11RasterizerContext::SetMaterial(CKMaterialData *mat) { return CKRasterizerContext::SetMaterial(mat); }

CKBOOL CKDX11RasterizerContext::SetViewport(CKViewportData *data) {
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = data->ViewX;
    viewport.TopLeftY = data->ViewY;
    viewport.Width = data->ViewWidth;
    viewport.Height = data->ViewHeight;
    viewport.MaxDepth = data->ViewZMax;
    viewport.MinDepth = data->ViewZMin;
    m_DeviceContext->RSSetViewports(1, &viewport);
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    return CKRasterizerContext::SetTransformMatrix(Type, Mat);
}
CKBOOL CKDX11RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    return CKRasterizerContext::SetRenderState(State, Value);
}
CKBOOL CKDX11RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}
CKBOOL CKDX11RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
    return CKRasterizerContext::SetTexture(Texture, Stage);
}
CKBOOL CKDX11RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    return CKRasterizerContext::SetTextureStageState(Stage, Tss, Value);
}
CKBOOL CKDX11RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    return CKRasterizerContext::SetVertexShader(VShaderIndex);
}
CKBOOL CKDX11RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    return CKRasterizerContext::SetPixelShader(PShaderIndex);
}
CKBOOL CKDX11RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}
CKBOOL CKDX11RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}
CKBOOL CKDX11RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount,
                                              VxDrawPrimitiveData *data)
{
    return CKRasterizerContext::DrawPrimitive(pType, indices, indexcount, data);
}
CKBOOL CKDX11RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
                                                CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
    return CKRasterizerContext::DrawPrimitiveVB(pType, VertexBuffer, StartIndex, VertexCount, indices, indexcount);
}
CKBOOL CKDX11RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                                  CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
    return CKRasterizerContext::DrawPrimitiveVBIB(pType, VB, IB, MinVIndex, VertexCount, StartIndex, Indexcount);
}
CKBOOL CKDX11RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    return CKRasterizerContext::CreateObject(ObjIndex, Type, DesiredFormat);
}
void *CKDX11RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
                                                CKRST_LOCKFLAGS Lock)
{
    return CKRasterizerContext::LockVertexBuffer(VB, StartVertex, VertexCount, Lock);
}
CKBOOL CKDX11RasterizerContext::UnlockVertexBuffer(CKDWORD VB) { return CKRasterizerContext::UnlockVertexBuffer(VB); }
CKBOOL CKDX11RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    return CKRasterizerContext::LoadTexture(Texture, SurfDesc, miplevel);
}
CKBOOL CKDX11RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}
CKBOOL CKDX11RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face,
                                                 CKBOOL GenerateMipMap)
{
    return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face, GenerateMipMap);
}
CKBOOL CKDX11RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    return CKRasterizerContext::DrawSprite(Sprite, src, dst);
}
int CKDX11RasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyToMemoryBuffer(rect, buffer, img_desc);
}
int CKDX11RasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyFromMemoryBuffer(rect, buffer, img_desc);
}
CKBOOL CKDX11RasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)
{
    return CKRasterizerContext::SetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}
CKBOOL CKDX11RasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)
{
    return CKRasterizerContext::GetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}
void *CKDX11RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    return CKRasterizerContext::LockIndexBuffer(IB, StartIndex, IndexCount, Lock);
}
CKBOOL CKDX11RasterizerContext::UnlockIndexBuffer(CKDWORD IB) { return CKRasterizerContext::UnlockIndexBuffer(IB); }
CKBOOL CKDX11RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat) { return 0; }
CKBOOL CKDX11RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat) { return 0; }
CKBOOL CKDX11RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat) { return 0; }
CKBOOL CKDX11RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat) { return 0; }
CKBOOL CKDX11RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat) { return 0; }
