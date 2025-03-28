#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"
#include "EnumMaps.h"

#include "VShader2DColor1.h"
#include "VShader2DColor2.h"
#include "VShaderColor.h"
#include "VShaderColor1Tex1.h"
#include "VShaderColor1Tex2.h"
#include "VShaderColor2Tex1.h"
#include "VShaderColor2Tex2.h"
#include "VShaderNormalTex1.h"
#include "VShaderNormalTex2.h"
#include "VShaderTex.h"
#include "PShader.h"

#include <algorithm>

#if defined(DEBUG) || defined(_DEBUG)
    #define LOGGING 1
    #define LOG_IA 0
    #define LOG_RENDERSTATE 0
    #define LOG_ALPHAFLAG 0
    #define UNHANDLED_TEXSTATE 0
    #define STATUS 1
    #define VB_STRICT 0
#endif

#if LOG_IA
#include <conio.h>
static bool step_mode = false;
#endif

#if STATUS
static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;
#endif

extern void debug_setup(CKDX11RasterizerContext *rst);
extern void debug_destroy();

void flag_toggle(uint32_t *state_dword, uint32_t flag, bool enabled)
{
    if (enabled)
        *state_dword |= flag;
    else
        *state_dword &= ~0U ^ flag;
}

void InverseMatrix(VxMatrix& result, const VxMatrix &m)
{
    using namespace DirectX;

    // XMFLOAT4X4 src((const float*) &m);
    XMMATRIX srcmat = XMLoadFloat4x4((XMFLOAT4X4*)&m);

    XMMATRIX invmat = XMMatrixInverse(nullptr, srcmat);

    XMFLOAT4X4 *dest = (XMFLOAT4X4 *)&result;
    XMStoreFloat4x4(dest, invmat);
}

CKDX11RasterizerContext::CKDX11RasterizerContext() { CKRasterizerContext::CKRasterizerContext(); }
CKDX11RasterizerContext::~CKDX11RasterizerContext()
{
    TracyD3D11Destroy(g_D3d11Ctx);
    debug_destroy();
}

void CKDX11RasterizerContext::resize_buffers()
{
#if defined(DEBUG) || defined(_DEBUG)
    fprintf(stderr, "resize_buffers\n");
#endif
    if (!m_Swapchain)
        return;
    HRESULT hr;
    m_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_RenderTargetView.Reset();
    D3DCall(m_Swapchain->ResizeBuffers(0, m_Width, m_Height, DXGI_FORMAT_UNKNOWN,
                                       (m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)));
    
    ComPtr<ID3D11Texture2D> backBuffer;
    D3DCall(m_Swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    D3DCall(m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_RenderTargetView.ReleaseAndGetAddressOf()));
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), nullptr);
}

void CKDX11RasterizerContext::toggle_fullscreen(bool fullscreen) {
#if defined(DEBUG) || defined(_DEBUG)
    fprintf(stderr, "toggle fullscreen: %s\n", fullscreen ? "on" : "off");
#endif
    HRESULT hr;
    LONG style = GetWindowLongA((HWND)m_Window, GWL_STYLE);
    // WIN_HANDLE parent = VxGetParent(m_Window);
    if (m_Fullscreen)
    {
        style &= ~WS_CHILD;
        style &= ~WS_CAPTION;
    } else
    {
        // style |= WS_CHILD;
        style |= WS_CAPTION;
    }
    SetWindowLongA((HWND)m_Window, GWL_STYLE, style);
    // style = GetWindowLongA((HWND)parent, GWL_STYLE);
    // SetWindowLongA((HWND)parent, GWL_STYLE, style & (~WS_CAPTION));
    // D3DCall(m_Swapchain->SetFullscreenState(m_Fullscreen, nullptr));
    BOOL cur_state;
    D3DCall(m_Swapchain->GetFullscreenState(&cur_state, nullptr));
    if (m_Fullscreen != cur_state)
    {
        ShowWindow((HWND)m_Window, SW_MINIMIZE);
        ShowWindow((HWND)m_Window, SW_RESTORE);
        hr = m_Swapchain->SetFullscreenState(m_Fullscreen, nullptr);
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
        {
            m_Fullscreen = !m_Fullscreen;
            m_Owner->m_FullscreenContext = m_Fullscreen ? this : nullptr;
            return;
        }
    }
    // ComPtr<ID3D11Debug> debug;
    // D3DCall(m_Device->QueryInterface(IID_PPV_ARGS(debug.GetAddressOf())));
    // D3DCall(debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL));
    m_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_RenderTargetView.Reset();
    D3DCall(m_Swapchain->ResizeBuffers(
        0, m_Width, m_Height, DXGI_FORMAT_UNKNOWN,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
        (m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0)));
    ComPtr<ID3D11Texture2D> backBuffer;
    D3DCall(m_Swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    D3DCall(m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_RenderTargetView.ReleaseAndGetAddressOf()));
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), nullptr);
}

void CKDX11RasterizerContext::SetTitleStatus(const char *fmt, ...)
{
    std::string ts;
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        va_list argsx;
        va_copy(argsx, args);
        ts.resize(vsnprintf(NULL, 0, fmt, argsx) + 1);
        va_end(argsx);
        vsnprintf(ts.data(), ts.size(), fmt, args);
        va_end(args);
        ts = m_OriginalTitle + " | " + ts;
    }
    else
        ts = m_OriginalTitle;

    SetWindowTextA(GetAncestor((HWND)m_Window, GA_ROOT), ts.c_str());
}

HRESULT CKDX11RasterizerContext::CreateSwapchain(WIN_HANDLE Window, int Width, int Height)
{
    HRESULT hr;
    m_AllowTearing = static_cast<CKDX11Rasterizer *>(m_Owner)->m_TearingSupport;
    m_FlipPresent = static_cast<CKDX11Rasterizer *>(m_Owner)->m_FlipPresent;
    ComPtr<IDXGIFactory2> factory2;
    HRESULT hr_dxgi12 = m_Owner->m_Factory.As(&factory2);
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (FAILED(hr_dxgi12))
    {
        DXGI_SWAP_CHAIN_DESC scd{};

        scd.BufferCount = 2;
        scd.BufferDesc.Width = Width;
        scd.BufferDesc.Height = Height;
        scd.BufferDesc.Format = format; // TODO: just use 32-bit color here, too lazy to check if valid
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = (HWND)Window;
        scd.SampleDesc.Count = 1; // TODO: multisample support
        scd.Windowed = TRUE;
        scd.SwapEffect = m_FlipPresent ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL : DXGI_SWAP_EFFECT_DISCARD;
        scd.Flags =
            // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | 
            (m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        D3DCall(m_Owner->m_Factory->CreateSwapChain(m_Device.Get(), &scd, m_Swapchain.ReleaseAndGetAddressOf()));
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC1 scd{};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd{};

        scd.Width = 0;
        scd.Height = 0;
        scd.Format = format;
        scd.Stereo = FALSE;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = 2;
        scd.Scaling = DXGI_SCALING_NONE;
        scd.SwapEffect = m_FlipPresent ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL : DXGI_SWAP_EFFECT_DISCARD;
        scd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        scd.Flags =
            // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
            (m_AllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

        scfd.RefreshRate.Numerator = 0;
        scfd.RefreshRate.Denominator = 0;
        scfd.Scaling = DXGI_MODE_SCALING_STRETCHED;
        scfd.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        scfd.Windowed = TRUE;
        IDXGISwapChain1 *swapchain1 = nullptr;
        D3DCall(factory2->CreateSwapChainForHwnd(m_Device.Get(), (HWND)Window, &scd, &scfd, nullptr, &swapchain1));
        m_Swapchain = swapchain1;
    }
    return hr;
}

HRESULT CKDX11RasterizerContext::CreateDevice()
{
    HRESULT hr;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
        // D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_1 // Sry but we can't do fl 9.x
    };
#if defined(DEBUG) || defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(static_cast<CKDX11RasterizerDriver *>(m_Driver)->m_Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
                           nullptr, creationFlags, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
                           m_Device.ReleaseAndGetAddressOf(), nullptr, m_DeviceContext.ReleaseAndGetAddressOf());
    if (hr == E_INVALIDARG)
    {
        D3DCall(D3D11CreateDevice(static_cast<CKDX11RasterizerDriver *>(m_Driver)->m_Adapter.Get(),
                                  D3D_DRIVER_TYPE_UNKNOWN, nullptr, creationFlags, &featureLevels[1],
                                  _countof(featureLevels) - 1, D3D11_SDK_VERSION, m_Device.ReleaseAndGetAddressOf(),
                                  nullptr, m_DeviceContext.ReleaseAndGetAddressOf()));
    }
    
    return hr;
}

CKBOOL CKDX11RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
                                       CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
#if (LOGGING)
    if (Window != m_Window)
        fprintf(stderr, "Not the same window! prev: 0x%x, cur: 0x%x\n", m_Window, Window);
#endif
    HRESULT hr;
    D3DCall(m_Owner->m_Factory->MakeWindowAssociation((HWND)Window, DXGI_MWA_NO_ALT_ENTER));
    
    if (m_Inited)
    {
        if (m_Fullscreen == Fullscreen)
            return TRUE;
        m_Fullscreen = Fullscreen;
        if (Fullscreen)
            m_Owner->m_FullscreenContext = this;
        else
            m_Owner->m_FullscreenContext = nullptr;
        toggle_fullscreen(Fullscreen);
        m_InCreateDestroy = FALSE;
        return TRUE;
    }
#if (LOGGING)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
    m_InCreateDestroy = TRUE;
    debug_setup(this);

    CKRECT Rect;
    WIN_HANDLE parent = VxGetParent(Window);
    if (Window)
    {
        VxGetWindowRect(Window, &Rect);
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&Rect));
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&Rect.right));
    }
    LONG PrevStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
    SetWindowLongA((HWND)Window, GWL_STYLE, PrevStyle & ~WS_CHILDWINDOW);
    SetWindowLongA((HWND)Window, GWL_STYLE, PrevStyle | (Fullscreen ? 0 : WS_CAPTION));

    D3DCall(CreateDevice());
    D3DCall(CreateSwapchain(Window, Width, Height));
#if TRACY_ENABLE
    g_D3d11Ctx = TracyD3D11Context(m_Device.Get(), m_DeviceContext.Get());
#endif
    

    ID3D11Texture2D *pBuffer = nullptr;
    D3DCall(m_Swapchain->GetBuffer(0, IID_PPV_ARGS(&pBuffer)));
    D3DCall(m_Device->CreateRenderTargetView(pBuffer, nullptr, m_RenderTargetView.ReleaseAndGetAddressOf()));
    D3DCall(pBuffer->Release());
    
    pBuffer = nullptr;
    D3D11_TEXTURE2D_DESC descDepth;
    descDepth.Width = Width;
    descDepth.Height = Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // TODO: heh, also too lazy to check
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    D3DCall(m_Device->CreateTexture2D(&descDepth, NULL, &pBuffer));

    // D3D11_DEPTH_STENCIL_DESC dsDesc;

    // Depth test parameters
    m_DepthStencilDesc.DepthEnable = TRUE;
    m_DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    // Stencil test parameters
    m_DepthStencilDesc.StencilEnable = TRUE;
    m_DepthStencilDesc.StencilReadMask = 0xFF;
    m_DepthStencilDesc.StencilWriteMask = 0xFF;

    // Stencil operations if pixel is front-facing
    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    m_DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    m_DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Stencil operations if pixel is back-facing
    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    m_DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    m_DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    
    // Create depth stencil state
    D3DCall(m_Device->CreateDepthStencilState(&m_DepthStencilDesc, m_DepthStencilState.GetAddressOf()));
    // Bind depth stencil state
    m_DeviceContext->OMSetDepthStencilState(m_DepthStencilState.Get(), 1);

    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV{};
    descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    D3DCall(m_Device->CreateDepthStencilView(pBuffer, &descDSV, m_DepthStencilView.GetAddressOf()));
    D3DCall(pBuffer->Release());
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), m_DepthStencilView.Get());

    // D3D11_SAMPLER_DESC SamplerDesc = {};

    VxColor border(1.0f, 1.0f, 1.0f, 1.0f);
    for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        m_SamplerDesc[i].Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        m_SamplerDesc[i].AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        m_SamplerDesc[i].AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        m_SamplerDesc[i].AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        m_SamplerDesc[i].MipLODBias = 0.0f;
        m_SamplerDesc[i].MaxAnisotropy = 1;
        m_SamplerDesc[i].ComparisonFunc = D3D11_COMPARISON_NEVER;
        std::copy_n(border.col, 4, m_SamplerDesc[i].BorderColor);
        m_SamplerDesc[i].MinLOD = -FLT_MAX;
        m_SamplerDesc[i].MaxLOD = FLT_MAX;
        D3DCall(m_Device->CreateSamplerState(&m_SamplerDesc[i], m_SamplerState[i].GetAddressOf()));
    }

    ID3D11SamplerState *samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        samplers[i] = m_SamplerState[i].Get();
    }
    // m_DeviceContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_SamplerState->GetAddressOf());
    m_DeviceContext->PSSetSamplers(0, static_cast<UINT>(std::size(samplers)), samplers);

    ZeroMemory(&m_BlendStateDesc, sizeof(D3D11_BLEND_DESC));
    m_BlendStateDesc.AlphaToCoverageEnable = FALSE;
    m_BlendStateDesc.IndependentBlendEnable = FALSE;
    m_BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    m_BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    m_BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3DCall(m_Device->CreateBlendState(&m_BlendStateDesc, m_BlendState.GetAddressOf()));
    m_DeviceContext->OMSetBlendState(m_BlendState.Get(), NULL, 0xFFFFFF);

    m_RasterizerDesc.AntialiasedLineEnable = false;
    m_RasterizerDesc.CullMode = D3D11_CULL_FRONT;
    m_RasterizerDesc.DepthBias = 0;
    m_RasterizerDesc.DepthBiasClamp = 0.0f;
    m_RasterizerDesc.DepthClipEnable = true;
    m_RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    m_RasterizerDesc.FrontCounterClockwise = true;
    m_RasterizerDesc.MultisampleEnable = false;
    m_RasterizerDesc.ScissorEnable = false;
    m_RasterizerDesc.SlopeScaledDepthBias = 0.0f;
    D3DCall(m_Device->CreateRasterizerState(&m_RasterizerDesc, m_RasterizerState.GetAddressOf()));
    m_DeviceContext->RSSetState(m_RasterizerState.Get());
    m_RasterizerStateUpToDate = TRUE;
    
    if (Window && !Fullscreen)
    {
        VxMoveWindow(Window, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, FALSE);
    }

    m_Window = (HWND)Window;

    m_OriginalTitle.resize(GetWindowTextLengthA(GetAncestor((HWND)Window, GA_ROOT)) + 1);
    GetWindowTextA(GetAncestor((HWND)Window, GA_ROOT), m_OriginalTitle.data(), m_OriginalTitle.size());
    while (m_OriginalTitle.back() == '\0')
        m_OriginalTitle.pop_back();

    
    m_PosX = PosX;
    m_PosY = PosY;
    m_Fullscreen = Fullscreen;
    m_Bpp = Bpp;
    m_ZBpp = Zbpp;
    m_Width = Width;
    m_Height = Height;

    CKDWORD vs_2d_idx = 0, vs_2d_color2_idx = 9;
    CKDWORD vs_normal_tex1_idx = 1, vs_normal_tex2_idx = 2;
    CKDWORD vs_color1_tex1_idx = 3, vs_color1_tex2_idx = 4, vs_color2_tex1_idx = 5, vs_color2_tex2_idx = 6;
    CKDWORD vs_tex_idx = 7, vs_color_idx = 8;
    CKDWORD VS_MAX = 9;
    CKDWORD ps_idx = 0;
    CKDX11VertexShaderDesc vs_2d_desc;
    vs_2d_desc.m_Function = (CKDWORD *)g_VShader2DColor1;
    vs_2d_desc.m_FunctionSize = sizeof(g_VShader2DColor1);
    vs_2d_desc.DxFVF = CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_TEX1;
    CreateObject(vs_2d_idx, CKRST_OBJ_VERTEXSHADER, &vs_2d_desc);

    CKDX11VertexShaderDesc vs_2d_color2_desc;
    vs_2d_color2_desc.m_Function = (CKDWORD *)g_VShader2DColor2;
    vs_2d_color2_desc.m_FunctionSize = sizeof(g_VShader2DColor2);
    vs_2d_color2_desc.DxFVF = CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1;
    CreateObject(vs_2d_color2_idx, CKRST_OBJ_VERTEXSHADER, &vs_2d_color2_desc);

    CKDX11PixelShaderDesc ps_desc;
    ps_desc.m_Function = (CKDWORD *)g_PShader;
    ps_desc.m_FunctionSize = sizeof(g_PShader);
    CreateObject(ps_idx, CKRST_OBJ_PIXELSHADER, &ps_desc);

    CKDX11VertexShaderDesc vs_normal_tex1;
    vs_normal_tex1.m_Function = (CKDWORD *)g_VShaderNormalTex1;
    vs_normal_tex1.m_FunctionSize = sizeof(g_VShaderNormalTex1);
    vs_normal_tex1.DxFVF = CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX1;
    CreateObject(vs_normal_tex1_idx, CKRST_OBJ_VERTEXSHADER, &vs_normal_tex1);

    CKDX11VertexShaderDesc vs_normal_tex2;
    vs_normal_tex2.m_Function = (CKDWORD *)g_VShaderNormalTex2;
    vs_normal_tex2.m_FunctionSize = sizeof(g_VShaderNormalTex2);
    vs_normal_tex2.DxFVF = CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX2;
    CreateObject(vs_normal_tex2_idx, CKRST_OBJ_VERTEXSHADER, &vs_normal_tex2);

    CKDX11VertexShaderDesc vs_color1_tex1;
    vs_color1_tex1.m_Function = (CKDWORD *)g_VShaderColor1Tex1;
    vs_color1_tex1.m_FunctionSize = sizeof(g_VShaderColor1Tex1);
    vs_color1_tex1.DxFVF = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX1;
    CreateObject(vs_color1_tex1_idx, CKRST_OBJ_VERTEXSHADER, &vs_color1_tex1);

    CKDX11VertexShaderDesc vs_color1_tex2;
    vs_color1_tex2.m_Function = (CKDWORD *)g_VShaderColor1Tex2;
    vs_color1_tex2.m_FunctionSize = sizeof(g_VShaderColor1Tex2);
    vs_color1_tex2.DxFVF = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX2;
    CreateObject(vs_color1_tex2_idx, CKRST_OBJ_VERTEXSHADER, &vs_color1_tex2);

    CKDX11VertexShaderDesc vs_color2_tex1;
    vs_color2_tex1.m_Function = (CKDWORD *)g_VShaderColor2Tex1;
    vs_color2_tex1.m_FunctionSize = sizeof(g_VShaderColor2Tex1);
    vs_color2_tex1.DxFVF = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1;
    CreateObject(vs_color2_tex1_idx, CKRST_OBJ_VERTEXSHADER, &vs_color2_tex1);

    CKDX11VertexShaderDesc vs_color2_tex2;
    vs_color2_tex2.m_Function = (CKDWORD *)g_VShaderColor2Tex2;
    vs_color2_tex2.m_FunctionSize = sizeof(g_VShaderColor2Tex2);
    vs_color2_tex2.DxFVF = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX2;
    CreateObject(vs_color2_tex2_idx, CKRST_OBJ_VERTEXSHADER, &vs_color2_tex2);
    
    CKDX11VertexShaderDesc vs_tex;
    vs_tex.m_Function = (CKDWORD *)g_VShaderTex;
    vs_tex.m_FunctionSize = sizeof(g_VShaderTex);
    vs_tex.DxFVF = CKRST_VF_POSITION | CKRST_VF_TEX1;
    CreateObject(vs_tex_idx, CKRST_OBJ_VERTEXSHADER, &vs_tex);

    CKDX11VertexShaderDesc vs_color;
    vs_color.m_Function = (CKDWORD *)g_VShaderColor;
    vs_color.m_FunctionSize = sizeof(g_VShaderColor);
    vs_color.DxFVF = CKRST_VF_POSITION | CKRST_VF_DIFFUSE;
    CreateObject(vs_color_idx, CKRST_OBJ_VERTEXSHADER, &vs_color);

    for (int i = 0; i <= VS_MAX; ++i)
    {
        if (m_VertexShaders[i])
            m_VertexShaderMap[static_cast<CKDX11VertexShaderDesc *>(m_VertexShaders[i])->DxFVF] = i;
    }

    // m_VertexShaderMap[CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_TEX1] = vs_2d_idx;
    // m_VertexShaderMap[CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1] = vs_2d_idx;
    //
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX1] = vs_normal_tex_idx;
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX2] = vs_normal_tex_idx;
    //
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_TEX1] = vs_tex_idx;
    //
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX1] = vs_color1_tex1_idx;
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1] = vs_color2_tex1_idx;
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1 | CKRST_VF_TEX2] = vs_color_tex2_idx;
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1 | CKRST_VF_TEX2] = vs_color_tex2_idx;
    //
    //
    // m_VertexShaderMap[CKRST_VF_POSITION | CKRST_VF_DIFFUSE] = vs_color_idx;
    // m_CurrentVShader = vs_idx;
    m_CurrentPShader = ps_idx;

    // auto *vs = static_cast<CKDX11VertexShaderDesc *>(m_VertexShaders[vs_idx]);
    auto *ps = static_cast<CKDX11PixelShaderDesc *>(m_PixelShaders[ps_idx]);
    //
    // vs->Bind(this);
    ps->Bind(this);

    m_VSConstantBuffer.Create(this, sizeof(VSConstantBufferStruct));
    m_PSConstantBuffer.Create(this, sizeof(PSConstantBufferStruct));
    m_PSLightConstantBuffer.Create(this, sizeof(PSLightConstantBufferStruct));
    m_PSTexCombinatorConstantBuffer.Create(this, sizeof(PSTexCombinatorConstantBufferStruct));
    ZeroMemory(&m_VSCBuffer, sizeof(VSConstantBufferStruct));
    ZeroMemory(&m_PSCBuffer, sizeof(PSConstantBufferStruct));
    ZeroMemory(&m_PSLightCBuffer, sizeof(PSLightConstantBufferStruct));
    ZeroMemory(&m_PSTexCombinatorCBuffer, sizeof(PSTexCombinatorConstantBufferStruct));

    // CKTextureDesc blank;
    // blank.Format.Width = 1;
    // blank.Format.Height = 1;
    // blank.Format.AlphaMask = 0xFF000000;
    // blank.Format.RedMask = 0x0000FF;
    // blank.Format.GreenMask = 0x00FF00;
    // blank.Format.BlueMask = 0xFF0000;
    // blank.MipMapCount = 0;
    // CreateTexture(0, &blank);
    // CKDWORD white = ~0U;
    // auto dx11tex = static_cast<CKDX11TextureDesc *>(m_Textures[0]);
    // if (!dx11tex->Create(this, &white))
    //     return FALSE;
    m_PSCBuffer.NullTextureMask = ~0U;
    m_PSConstantBufferUpToDate = FALSE;

    SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, 1);
    SetRenderState(VXRENDERSTATE_LOCALVIEWER, 1);
    SetRenderState(VXRENDERSTATE_COLORVERTEX, 0);

    SetTitleStatus("D3D11 | DXGI %s | AllowTearing: %s",
                   m_Owner->m_DXGIVersionString.c_str(), m_AllowTearing ? "true" : "false");

    // if (Fullscreen && !m_Driver->m_Owner->m_FullscreenContext)
    // {
    //     toggle_fullscreen();
    //     m_Driver->m_Owner->m_FullscreenContext = this;
    // }
    m_InCreateDestroy = FALSE;
    m_Inited = TRUE;

    return SUCCEEDED(hr);
}
CKBOOL CKDX11RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    CKRECT Rect;
    if (m_Window)
    {
        VxGetWindowRect(m_Window, &Rect);
        WIN_HANDLE Parent = VxGetParent(m_Window);
        VxScreenToClient(Parent, reinterpret_cast<CKPOINT *>(&Rect));
        VxScreenToClient(Parent, reinterpret_cast<CKPOINT *>(&Rect.right));
    }
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount,
                                      CKRECT *rects)
{
    if (!m_RenderTargetView)
        return FALSE;
    if (Flags & CKRST_CTXCLEAR_COLOR)
    {
        VxColor c(Ccol);
        m_DeviceContext->ClearRenderTargetView(m_RenderTargetView.Get(), (const float*)&c);
    }
    UINT dsClearFlag = 0;
    if (Flags & CKRST_CTXCLEAR_DEPTH)
        dsClearFlag |= D3D11_CLEAR_DEPTH;
    if (Flags & CKRST_CTXCLEAR_STENCIL)
        dsClearFlag |= D3D11_CLEAR_STENCIL;
    if (dsClearFlag)
        m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), dsClearFlag, Z, Stencil);
    
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::BackToFront(CKBOOL vsync) {
    FrameMark;

    if (m_InCreateDestroy)
        return TRUE;
    if (!m_SceneBegined)
        EndScene();
    HRESULT hr;
#if STATUS
    // fprintf(stderr, "swap\n");
    SetTitleStatus("D3D11 | DXGI %s | AllowTearing: %s | batch stats: direct %d, vb %d, vbib %d", 
        m_Owner->m_DXGIVersionString.c_str(), m_AllowTearing ? "true" : "false", directbat, vbbat, vbibbat);

    directbat = 0;
    vbbat = 0;
    vbibbat = 0;
#endif
    // if (m_NeedBufferResize)
    // {
    //     resize_buffers();
    //     m_NeedBufferResize = FALSE;
    // }
    D3DCall(m_Swapchain->Present(vsync ? 1 : 0,
                             (m_AllowTearing && !m_Fullscreen && !vsync)
                                 ? DXGI_PRESENT_ALLOW_TEARING
                                 : 0));
    // D3DCall(m_Swapchain->Present(vsync ? 1 : 0, 0));
    TracyD3D11Collect(g_D3d11Ctx);
    return SUCCEEDED(hr);
}

CKBOOL CKDX11RasterizerContext::BeginScene()
{
    if (m_SceneBegined)
        return FALSE;
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), m_DepthStencilView.Get());
    m_SceneBegined = TRUE;
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::EndScene()
{
    if (!m_SceneBegined)
        return FALSE;
    m_MatrixUptodate = 0;
    m_VSConstantBufferUpToDate = FALSE;
    m_SceneBegined = FALSE;
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    if (Light >= MAX_ACTIVE_LIGHTS)
        return FALSE;
    m_PSLightConstantBufferUpToDate = FALSE;
    m_CurrentLightData[Light] = *data;
    bool enabled = (m_PSLightCBuffer.Lights[Light].type & LFLG_LIGHTEN);
    m_PSLightCBuffer.Lights[Light] = *data;
    flag_toggle(&m_PSLightCBuffer.Lights[Light].type, LFLG_LIGHTEN, enabled);
    // ConvertAttenuationModelFromDX5(m_PSCBuffer.Lights[Light].a0, m_PSCBuffer.Lights[Light].a1,
    //                                m_PSCBuffer.Lights[Light].a2,
    //                                data->Range);
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    if (Light >= MAX_ACTIVE_LIGHTS)
        return FALSE;
    m_PSLightConstantBufferUpToDate = FALSE;
    flag_toggle(&m_PSLightCBuffer.Lights[Light].type, LFLG_LIGHTEN, Enable);
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::SetMaterial(CKMaterialData *mat)
{
    m_CurrentMaterialData = *mat;
    m_PSConstantBufferUpToDate = FALSE;
    m_PSCBuffer.Material = *mat;
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::SetViewport(CKViewportData *data) {
    m_Viewport.TopLeftX = (FLOAT)data->ViewX;
    m_Viewport.TopLeftY = (FLOAT)data->ViewY;
    m_Viewport.Width = (FLOAT)data->ViewWidth;
    m_Viewport.Height = (FLOAT)data->ViewHeight;
    m_Viewport.MaxDepth = 1.0f;
    m_Viewport.MinDepth = 0.0f;
    m_DeviceContext->RSSetViewports(1, &m_Viewport);
    
    m_VSConstantBufferUpToDate = FALSE;
    m_VSCBuffer.ViewportMatrix = VxMatrix::Identity();
    float(*m)[4] = (float(*)[4]) &m_VSCBuffer.ViewportMatrix;
    m[0][0] = 2. / data->ViewWidth;
    m[1][1] = 2. / data->ViewHeight;
    m[2][2] = 0;
    m[3][0] = -(-2. * data->ViewX + data->ViewWidth) / data->ViewWidth;
    m[3][1] = (-2. * data->ViewY + data->ViewHeight) / data->ViewHeight;
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    ZoneScopedN(__FUNCTION__);
    CKDWORD UnityMatrixMask = 0;
     switch (Type)
     {
         case VXMATRIX_PROJECTION:
         {
             float(*m)[4] = (float(*)[4]) & Mat;
             float A = m[2][2];
             float B = m[3][2];
             m_PSCBuffer.DepthRange[0] = -B / A;
             m_PSCBuffer.DepthRange[1] = B / (1 - A); // for eye-distance fog calculation
             m_PSConstantBufferUpToDate = FALSE;
             break;
         }
         case VXMATRIX_TEXTURE0:
         case VXMATRIX_TEXTURE1:
         case VXMATRIX_TEXTURE2:
         case VXMATRIX_TEXTURE3:
         case VXMATRIX_TEXTURE4:
         case VXMATRIX_TEXTURE5:
         case VXMATRIX_TEXTURE6:
         case VXMATRIX_TEXTURE7:
        {
            UnityMatrixMask = TEXTURE0_TRANSFORM << (Type - TEXTURE1_TRANSFORM);
            CKDWORD tex = Type - VXMATRIX_TEXTURE0;
            Vx3DTransposeMatrix(m_VSCBuffer.TexTransformMatrix[tex], Mat);
            // m_VSCBuffer.TexTransformMatrix[tex] = Mat;
            m_VSConstantBufferUpToDate = FALSE;
            break;
        }
         default:
             break;
     }
    auto ret = CKRasterizerContext::SetTransformMatrix(Type, Mat);
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= UnityMatrixMask;
    }
    else
    {
        m_UnityMatrixMask &= ~UnityMatrixMask;
    }
    m_VSConstantBufferUpToDate = FALSE;
    return ret;
}
CKBOOL CKDX11RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    if (m_StateCache[State].Flags)
        return TRUE;

    if (m_StateCache[State].Valid && m_StateCache[State].Value == Value)
    {
        ++m_RenderStateCacheHit;
        return TRUE;
    }

#if LOGGING && LOG_RENDERSTATE
    if (State == VXRENDERSTATE_SRCBLEND || State == VXRENDERSTATE_DESTBLEND)
    fprintf(stderr, "set render state %s -> %x, currently %x %s\n", rstytostr(State), Value,
            m_StateCache[State].Valid ? m_StateCache[State].Value : m_StateCache[State].DefaultValue,
            m_StateCache[State].Valid ? "" : "[Invalid]");
#endif
    ++m_RenderStateCacheMiss;
    m_StateCache[State].Value = Value;
    m_StateCache[State].Valid = 1;

    if (State < m_StateCacheMissMask.Size() && m_StateCacheMissMask.IsSet(State))
        return FALSE;

    return InternalSetRenderState(State, Value);
}

CKBOOL CKDX11RasterizerContext::InternalSetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    switch (State)
    {
        case VXRENDERSTATE_ANTIALIAS:
            m_RasterizerStateUpToDate = FALSE;
            m_RasterizerDesc.MultisampleEnable = Value;
            return TRUE;
        case VXRENDERSTATE_TEXTUREPERSPECTIVE:
            return FALSE;
        case VXRENDERSTATE_ZENABLE:
            m_DepthStencilStateUpToDate = FALSE;
            // m_DepthStencilDesc.DepthEnable = (BOOL)Value;
            m_DepthStencilDesc.DepthEnable = TRUE;
            return TRUE;
        case VXRENDERSTATE_FILLMODE:
            m_RasterizerStateUpToDate = FALSE;
            switch ((VXFILL_MODE)Value)
            {
                case VXFILL_POINT:
                    // not supported.
                    return FALSE;
                case VXFILL_WIREFRAME:
                    m_RasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
                    break;
                case VXFILL_SOLID:
                    m_RasterizerDesc.FillMode = D3D11_FILL_SOLID;
                    break;
            }
            return TRUE;
        case VXRENDERSTATE_SHADEMODE:
            return FALSE;
        case VXRENDERSTATE_LINEPATTERN:
            break;
        case VXRENDERSTATE_ZWRITEENABLE:
            m_DepthStencilStateUpToDate = FALSE;
            m_DepthStencilDesc.DepthWriteMask = Value ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            return TRUE;
        case VXRENDERSTATE_ALPHATESTENABLE:
            m_PSConstantBufferUpToDate = FALSE;
            flag_toggle(&m_PSCBuffer.AlphaFlags, AFLG_ALPHATESTEN, Value);
            return TRUE;
        case VXRENDERSTATE_SRCBLEND:
            m_BlendStateUpToDate = FALSE;
            switch ((VXBLEND_MODE) Value)
            {
                case VXBLEND_ZERO:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
                    break;
                case VXBLEND_ONE:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                    break;
                case VXBLEND_SRCCOLOR:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_COLOR;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
                    break;
                case VXBLEND_INVSRCCOLOR:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_SRC_COLOR;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                    break;
                case VXBLEND_SRCALPHA:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
                    break;
                case VXBLEND_INVSRCALPHA:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_SRC_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                    break;
                case VXBLEND_SRCALPHASAT:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA_SAT;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA_SAT;
                    break;
                case VXBLEND_DESTALPHA:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
                    break;
                case VXBLEND_INVDESTALPHA:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
                    break;
                case VXBLEND_DESTCOLOR:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
                    break;
                case VXBLEND_INVDESTCOLOR:
                    m_BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
                    m_BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
                    break;
                default:
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_DESTBLEND:
            m_BlendStateUpToDate = FALSE;
            switch ((VXBLEND_MODE)Value)
            {
                case VXBLEND_ZERO:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
                   break;
                case VXBLEND_ONE:
                   m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
                   break;
                case VXBLEND_SRCCOLOR:
                   m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                   break;
                case VXBLEND_INVSRCCOLOR:
                   m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                   break;
                case VXBLEND_SRCALPHA:
                   m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
                   break;
                case VXBLEND_INVSRCALPHA:
                   m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                   m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                   break;
                case VXBLEND_DESTALPHA:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
                    break;
                case VXBLEND_INVDESTALPHA:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_DEST_ALPHA;
                    m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
                    break;
                case VXBLEND_DESTCOLOR:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_COLOR;
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_ALPHA;
                    break;
                case VXBLEND_INVDESTCOLOR:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_DEST_COLOR;
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_DEST_ALPHA;
                    break;
                case VXBLEND_SRCALPHASAT:
                    m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_ALPHA_SAT;
                    m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA_SAT;
                    break;
            //     case VXBLEND_BOTHSRCALPHA:
            //         m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC1_ALPHA;
            //         // m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC1_ALPHA;
            //         break;
            //     case VXBLEND_BOTHINVSRCALPHA:
            //         m_BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_ALPHA;
            //         // m_BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
            //         break;
                default:
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_CULLMODE:
            m_RasterizerStateUpToDate = FALSE;
            switch ((VXCULL)Value)
            {
                case VXCULL_NONE:
                    m_RasterizerDesc.CullMode = D3D11_CULL_NONE;
                    break;
                case VXCULL_CW:
                    m_RasterizerDesc.CullMode = D3D11_CULL_FRONT;
                    break;
                case VXCULL_CCW:
                    m_RasterizerDesc.CullMode = D3D11_CULL_BACK;
                    break;
                default: 
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_ZFUNC:
            m_DepthStencilStateUpToDate = FALSE;
            switch ((VXCMPFUNC) Value)
            {
                case VXCMP_NEVER:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_NEVER;
                    break;
                case VXCMP_LESS:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
                    break;
                case VXCMP_EQUAL:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
                    break;
                case VXCMP_LESSEQUAL:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
                    break;
                case VXCMP_GREATER:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER;
                    break;
                case VXCMP_NOTEQUAL:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
                    break;
                case VXCMP_GREATEREQUAL:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
                    break;
                case VXCMP_ALWAYS:
                    m_DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
                    break;
                default:
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_ALPHAREF:
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.AlphaThreshold = Value / 255.;
            return TRUE;
        case VXRENDERSTATE_ALPHAFUNC:
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.AlphaFlags &= ~VXCMP_MASK;
            m_PSCBuffer.AlphaFlags |= (Value & VXCMP_MASK);
            return TRUE;
        case VXRENDERSTATE_DITHERENABLE:
            return FALSE;
        case VXRENDERSTATE_ALPHABLENDENABLE:
            m_BlendStateUpToDate = FALSE;
            m_BlendStateDesc.RenderTarget[0].BlendEnable = Value;
            return TRUE;
        case VXRENDERSTATE_FOGENABLE:
            // if ((bool)Value == (bool)(m_PSCBuffer.FogFlags & FFLG_FOGEN))
            //     return TRUE;
            m_PSConstantBufferUpToDate = FALSE;
            flag_toggle(&m_PSCBuffer.FogFlags, FFLG_FOGEN, Value);
            return TRUE;
        case VXRENDERSTATE_FOGCOLOR:
        {
            m_PSConstantBufferUpToDate = FALSE;
            VxColor col(Value);
            m_PSCBuffer.FogColor = col;
            return TRUE;
        }
        case VXRENDERSTATE_FOGPIXELMODE:
            if ((m_PSCBuffer.FogFlags & ~FFLG_FOGEN) == Value)
                return TRUE;
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.FogFlags = (m_PSCBuffer.FogFlags & FFLG_FOGEN) | Value;
            return TRUE;
        case VXRENDERSTATE_FOGSTART:
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.FogStart = reinterpret_cast<float&>(Value);
            return TRUE;
        case VXRENDERSTATE_FOGEND:
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.FogEnd = reinterpret_cast<float&>(Value);
            return TRUE;
        case VXRENDERSTATE_FOGDENSITY:
            m_PSConstantBufferUpToDate = FALSE;
            m_PSCBuffer.FogDensity = reinterpret_cast<float&>(Value);
            return TRUE;
        case VXRENDERSTATE_EDGEANTIALIAS:
            break;
        case VXRENDERSTATE_ZBIAS:
            break;
        case VXRENDERSTATE_RANGEFOGENABLE:
            break;
        case VXRENDERSTATE_STENCILENABLE:
            m_DepthStencilStateUpToDate = FALSE;
            m_DepthStencilDesc.StencilEnable = Value;
            return TRUE;
        case VXRENDERSTATE_STENCILFAIL:
            m_DepthStencilStateUpToDate = FALSE;
            switch ((VXSTENCILOP) Value)
            {
                case VXSTENCILOP_KEEP:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    break;
                case VXSTENCILOP_ZERO:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    break;
                case VXSTENCILOP_REPLACE:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    break;
                case VXSTENCILOP_INCRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    break;
                case VXSTENCILOP_DECRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    break;
                case VXSTENCILOP_INVERT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    break;
                case VXSTENCILOP_INCR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    break;
                case VXSTENCILOP_DECR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    break;
                default: return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_STENCILZFAIL:
            m_DepthStencilStateUpToDate = FALSE;
            switch ((VXSTENCILOP)Value)
            {
                case VXSTENCILOP_KEEP:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    break;
                case VXSTENCILOP_ZERO:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    break;
                case VXSTENCILOP_REPLACE:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    break;
                case VXSTENCILOP_INCRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    break;
                case VXSTENCILOP_DECRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    break;
                case VXSTENCILOP_INVERT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    break;
                case VXSTENCILOP_INCR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    break;
                case VXSTENCILOP_DECR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    break;
                default:
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_STENCILPASS:
            m_DepthStencilStateUpToDate = FALSE;
            switch ((VXSTENCILOP)Value)
            {
                case VXSTENCILOP_KEEP:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                    break;
                case VXSTENCILOP_ZERO:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
                    break;
                case VXSTENCILOP_REPLACE:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
                    break;
                case VXSTENCILOP_INCRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR_SAT;
                    break;
                case VXSTENCILOP_DECRSAT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR_SAT;
                    break;
                case VXSTENCILOP_INVERT:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INVERT;
                    break;
                case VXSTENCILOP_INCR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_INCR;
                    break;
                case VXSTENCILOP_DECR:
                    m_DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    m_DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_DECR;
                    break;
                default:
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_STENCILFUNC:
            m_DepthStencilStateUpToDate = FALSE;
            switch ((VXCMPFUNC) Value)
            {
                case VXCMP_NEVER:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
                    break;
                case VXCMP_LESS:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_LESS;
                    break;
                case VXCMP_EQUAL:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
                    break;
                case VXCMP_LESSEQUAL:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
                    break;
                case VXCMP_GREATER:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_GREATER;
                    break;
                case VXCMP_NOTEQUAL:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
                    break;
                case VXCMP_GREATEREQUAL:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL;
                    break;
                case VXCMP_ALWAYS:
                    m_DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                    m_DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                    break;
                default: return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_STENCILREF:
            break;
        case VXRENDERSTATE_STENCILMASK:
            m_DepthStencilStateUpToDate = FALSE;
            m_DepthStencilDesc.StencilReadMask = Value;
            return TRUE;
        case VXRENDERSTATE_STENCILWRITEMASK:
            m_DepthStencilStateUpToDate = FALSE;
            m_DepthStencilDesc.StencilWriteMask = Value;
            return TRUE;
        case VXRENDERSTATE_TEXTUREFACTOR:
            break;
        // case VXRENDERSTATE_WRAP0:
        //     switch ((VXWRAP_MODE) Value)
        //     {
        //         case VXWRAP_U:
        //             m_SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        //             break;
        //         case VXWRAP_V:
        //             m_SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        //             break;
        //         case VXWRAP_S:
        //             m_SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        //             break;
        //         case VXWRAP_T:
        //         default:
        //             return FALSE;
        //     }
        //     return TRUE;
        // case VXRENDERSTATE_WRAP1:
        //     break;
        // case VXRENDERSTATE_WRAP2:
        //     break;
        // case VXRENDERSTATE_WRAP3:
        //     break;
        // case VXRENDERSTATE_WRAP4:
        //     break;
        // case VXRENDERSTATE_WRAP5:
        //     break;
        // case VXRENDERSTATE_WRAP6:
        //     break;
        // case VXRENDERSTATE_WRAP7:
        //     break;
        case VXRENDERSTATE_CLIPPING:
            return FALSE;
        case VXRENDERSTATE_LIGHTING:
            m_PSConstantBufferUpToDate = FALSE;
            flag_toggle(&m_PSCBuffer.GlobalLightSwitches, LSW_LIGHTINGEN, Value);
            return TRUE;
        case VXRENDERSTATE_SPECULARENABLE:
            m_PSConstantBufferUpToDate = FALSE;
            flag_toggle(&m_PSCBuffer.GlobalLightSwitches, LSW_SPECULAREN, Value);
            return TRUE;
        case VXRENDERSTATE_AMBIENT:
            return FALSE;
        case VXRENDERSTATE_FOGVERTEXMODE:
            break;
        case VXRENDERSTATE_COLORVERTEX:
            m_PSConstantBufferUpToDate = FALSE;
            flag_toggle(&m_PSCBuffer.GlobalLightSwitches, LSW_VRTCOLOREN, Value);
            return TRUE;
        case VXRENDERSTATE_LOCALVIEWER:
            break;
        case VXRENDERSTATE_NORMALIZENORMALS:
            return FALSE;
        case VXRENDERSTATE_VERTEXBLEND:
            break;
        case VXRENDERSTATE_SOFTWAREVPROCESSING:
            return FALSE;
        case VXRENDERSTATE_CLIPPLANEENABLE:
            break;
        case VXRENDERSTATE_INDEXVBLENDENABLE:
            break;
        case VXRENDERSTATE_BLENDOP:
            m_BlendStateUpToDate = FALSE;
            switch ((VXBLENDOP) Value)
            {
                case VXBLENDOP_ADD:
                    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                    break;
                case VXBLENDOP_SUBTRACT:
                    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_SUBTRACT;
                    break;
                case VXBLENDOP_REVSUBTRACT:
                    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
                    break;
                case VXBLENDOP_MIN:
                    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
                    break;
                case VXBLENDOP_MAX:
                    m_BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
                    break;
                default: 
                    return FALSE;
            }
            return TRUE;
        case VXRENDERSTATE_TEXTURETARGET:
            break;
        case VXRENDERSTATE_INVERSEWINDING:
            m_RasterizerStateUpToDate = FALSE;
            m_InverseWinding = (Value != 0);
            m_RasterizerDesc.FrontCounterClockwise = m_InverseWinding;
            return TRUE;
        default: ;
    }
    return FALSE;
}

CKBOOL CKDX11RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}
CKBOOL CKDX11RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
#if LOG_SETTEXTURE
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKDX11TextureDesc *desc = static_cast<CKDX11TextureDesc *>(m_Textures[Texture]);
    if (!desc)
    {
        // desc = static_cast<CKDX11TextureDesc *>(m_Textures[0]);
        m_PSCBuffer.NullTextureMask |= (1 << Stage);
        m_PSConstantBufferUpToDate = FALSE;
        return TRUE;
        // ID3D11ShaderResourceView *srv_ptr = nullptr;
        // m_DeviceContext->PSSetShaderResources(0, 1, &srv_ptr);
        // return TRUE;
    }
    m_PSCBuffer.NullTextureMask &= ~(1 << Stage);
    m_PSConstantBufferUpToDate = FALSE;
    desc->Bind(this, Stage);
    return TRUE;
}

D3D11_TEXTURE_ADDRESS_MODE Vx2D3DTextureAddressMode(VXTEXTURE_ADDRESSMODE mode)
{
    switch (mode)
    {
        case VXTEXTURE_ADDRESSWRAP:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        case VXTEXTURE_ADDRESSMIRROR:
            return D3D11_TEXTURE_ADDRESS_MIRROR;
        case VXTEXTURE_ADDRESSCLAMP:
            return D3D11_TEXTURE_ADDRESS_CLAMP;
        case VXTEXTURE_ADDRESSBORDER:
            return D3D11_TEXTURE_ADDRESS_BORDER;
        case VXTEXTURE_ADDRESSMIRRORONCE:
            return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
        default: break;
    }
    return D3D11_TEXTURE_ADDRESS_WRAP;
}



CKBOOL CKDX11RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
        {
            m_SamplerStateUpToDate[Stage] = FALSE;
            auto mode = Vx2D3DTextureAddressMode((VXTEXTURE_ADDRESSMODE)Value);
            m_SamplerDesc[Stage].AddressU = mode;
            m_SamplerDesc[Stage].AddressV = mode;
            m_SamplerDesc[Stage].AddressW = mode;
            return TRUE;
        }
        case CKRST_TSS_ADDRESSU:
            m_SamplerStateUpToDate[Stage] = FALSE;
            m_SamplerDesc[Stage].AddressU = Vx2D3DTextureAddressMode((VXTEXTURE_ADDRESSMODE)Value);
            return TRUE;
        case CKRST_TSS_ADDRESSV:
            m_SamplerStateUpToDate[Stage] = FALSE;
            m_SamplerDesc[Stage].AddressV = Vx2D3DTextureAddressMode((VXTEXTURE_ADDRESSMODE)Value);
            return TRUE;
        case CKRST_TSS_BORDERCOLOR:
        {
            m_SamplerStateUpToDate[Stage] = FALSE;
            VxColor c(Value);
            std::copy_n(c.col, 4, m_SamplerDesc[Stage].BorderColor);
            return TRUE;
        }
        case CKRST_TSS_MINFILTER:
        case CKRST_TSS_MAGFILTER:
            m_SamplerStateUpToDate[Stage] = FALSE;
            return m_Filter[Stage].SetFilterMode(Tss, static_cast<VXTEXTURE_FILTERMODE>(Value));
        case CKRST_TSS_MIPMAPLODBIAS:
            m_SamplerStateUpToDate[Stage] = FALSE;
            m_SamplerDesc[Stage].MipLODBias = Value;
            return TRUE;
        case CKRST_TSS_MAXANISOTROPY:
            m_SamplerStateUpToDate[Stage] = FALSE;
            m_SamplerDesc[Stage].MaxAnisotropy = Value;
            return TRUE;
        case CKRST_TSS_STAGEBLEND:
            {
            CKDX11TexCombinatorConstant tc = m_PSTexCombinatorCBuffer.TexCombinator[Stage];
            bool valid = true;
            switch (Value)
            {
                    case STAGEBLEND(VXBLEND_ZERO, VXBLEND_SRCCOLOR):
                    case STAGEBLEND(VXBLEND_DESTCOLOR, VXBLEND_ZERO):
                        tc = CKDX11TexCombinatorConstant::make(TexOp::modulate, TexArg::texture, TexArg::current,
                                                            TexArg::current, TexOp::select1, TexArg::current,
                                                            TexArg::current, TexArg::current, tc.dest(), tc.constant);
                        break;
                    case STAGEBLEND(VXBLEND_ONE, VXBLEND_ONE):
                        tc = CKDX11TexCombinatorConstant::make(TexOp::add, TexArg::current, TexArg::current,
                                                            TexArg::current, TexOp::select1, TexArg::current,
                                                            TexArg::current, TexArg::current, tc.dest(), tc.constant);
                        break;
                    default:
                        valid = false;
            }
            if (valid)
            {
                    m_PSTexCombinatorCBuffer.TexCombinator[Stage] = tc;
                    m_PSTexCombinatorConstantBufferUpToDate = FALSE;
            }
            return valid;
            }
        case CKRST_TSS_TEXTUREMAPBLEND:
            {
            CKDX11TexCombinatorConstant tc = m_PSTexCombinatorCBuffer.TexCombinator[Stage];
            bool valid = true;
            switch (Value)
            {
                    case VXTEXTUREBLEND_DECAL:
                    case VXTEXTUREBLEND_COPY:
                        tc.set_color_op(TexOp::select1);
                        tc.set_color_arg1(TexArg::texture);
                        tc.set_alpha_op(TexOp::select1);
                        tc.set_alpha_arg1(TexArg::texture);
                        break;
                    case VXTEXTUREBLEND_MODULATE:
                    case VXTEXTUREBLEND_MODULATEALPHA:
                    case VXTEXTUREBLEND_MODULATEMASK:
                        tc = CKDX11TexCombinatorConstant::make(
                            TexOp::modulate, TexArg::texture, TexArg::current,
                                                            TexArg::current, TexOp::modulate, TexArg::texture,
                                                            TexArg::current, TexArg::current, tc.dest(), tc.constant);
                        break;
                    case VXTEXTUREBLEND_DECALALPHA:
                    case VXTEXTUREBLEND_DECALMASK:
                        tc.set_color_op(TexOp::mixtexalp);
                        tc.set_color_arg1(TexArg::texture);
                        tc.set_alpha_arg2(TexArg::current);
                        tc.set_alpha_op(TexOp::select1);
                        tc.set_alpha_arg1(TexArg::diffuse);
                        break;
                    case VXTEXTUREBLEND_ADD:
                        tc.set_color_op(TexOp::add);
                        tc.set_color_arg1(TexArg::texture);
                        tc.set_alpha_arg2(TexArg::current);
                        tc.set_alpha_op(TexOp::select1);
                        tc.set_alpha_arg1(TexArg::current);
                        break;
                    default:
                        valid = false;
            }
            if (valid)
            {
                    m_PSTexCombinatorCBuffer.TexCombinator[Stage] = tc;
                    m_PSTexCombinatorConstantBufferUpToDate = FALSE;
            }
            return valid;
            }
        case CKRST_TSS_TEXTURETRANSFORMFLAGS:
            {
            CKDWORD tvp = m_VSCBuffer.TextureTransformFlags[Stage];
            if (!Value)
                    tvp &= ~0U ^ TVP_TC_TRANSF;
            else
                    tvp |= TVP_TC_TRANSF;
            if (Value & CKRST_TTF_PROJECTED)
                    tvp |= TVP_TC_PROJECTED;
            else
                    tvp &= ~0U ^ TVP_TC_PROJECTED;
            if (tvp != m_VSCBuffer.TextureTransformFlags[Stage])
            {
                    m_VSCBuffer.TextureTransformFlags[Stage] = tvp;
                    m_VSConstantBufferUpToDate = FALSE;
            }
            return TRUE;
            }
        case CKRST_TSS_TEXCOORDINDEX:
            {
            // we currently ignore the texture coords index encoded in Value...
            // because we simply don't have that in our frag shader...
            // we only care about automatic texture coords generation for now.
            CKDWORD tvp = m_VSCBuffer.TextureTransformFlags[Stage] & (~0U ^ 0x07000000U);
            switch (Value >> 16)
            {
                    case 0:
                        break;
                    case 1:
                        tvp |= TVP_TC_CSNORM;
                        break;
                    case 2:
                        tvp |= TVP_TC_CSVECP;
                        break;
                    case 3:
                        tvp |= TVP_TC_CSREFV;
                        break;
                    default:
                        return FALSE;
            }
            if (tvp != m_VSCBuffer.TextureTransformFlags[Stage])
            {
                    m_VSCBuffer.TextureTransformFlags[Stage] = tvp;
                    m_VSConstantBufferUpToDate = FALSE;
            }
            return TRUE;
            }
        default:
#if UNHANDLED_TEXSTATE
            fprintf(stderr, "unhandled texture stage state %s -> %d\n", tstytostr(Tss), Value);
#endif
            return FALSE;
    }
}
CKBOOL CKDX11RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
//     if (m_CurrentVShader == VShaderIndex)
//         return TRUE;
//     if (VShaderIndex >= m_VertexShaders.Size())
//         return FALSE;
//     auto *vs = static_cast<CKDX11VertexShaderDesc *>(m_VertexShaders[VShaderIndex]);
//     if (!vs)
//         return FALSE;
// #if LOG_IA
//     fprintf(stderr, "IA: vs %s\n", vs->DxEntryPoint);
// #endif
//     vs->Bind(this);
//     m_CurrentVShader = VShaderIndex;
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    if (m_CurrentPShader == PShaderIndex)
        return TRUE;
    if (PShaderIndex >= m_PixelShaders.Size())
        return FALSE;
    auto *ps = static_cast<CKDX11VertexShaderDesc *>(m_VertexShaders[PShaderIndex]);
    if (!ps)
        return FALSE;
    ps->Bind(this);
    m_CurrentPShader = PShaderIndex;
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}
CKBOOL CKDX11RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}


CKDX11IndexBufferDesc* CKDX11RasterizerContext::GenerateIB(void *indices, int indexCount, int *startIndex)
{
    ZoneScopedN(__FUNCTION__);
    if (!indices)
        return nullptr;
    CKDX11IndexBufferDesc *ibo = nullptr;
    void *pdata = nullptr;
    auto iboid = m_DynamicIndexBufferCounter++;
    if (m_DynamicIndexBufferCounter >= DYNAMIC_IBO_COUNT)
        m_DynamicIndexBufferCounter = 0;
    if (!m_DynamicIndexBuffer[iboid] || m_DynamicIndexBuffer[iboid]->m_MaxIndexCount < indexCount)
    {
        if (m_DynamicIndexBuffer[iboid])
            delete m_DynamicIndexBuffer[iboid];
        ibo = new CKDX11IndexBufferDesc;
        ibo->m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        ibo->m_MaxIndexCount = indexCount + 100 < DEFAULT_VB_SIZE ? DEFAULT_VB_SIZE : indexCount + 100;
        ibo->m_CurrentICount = 0;
        if (!ibo->Create(this))
        {
            m_DynamicIndexBuffer[iboid] = nullptr;
            return FALSE;
        }
        m_DynamicIndexBuffer[iboid] = ibo;
    }
    ibo = m_DynamicIndexBuffer[iboid];
    if (indexCount + ibo->m_CurrentICount <= ibo->m_MaxIndexCount)
    {
        pdata = ibo->Lock(this, sizeof(CKWORD) * ibo->m_CurrentICount, sizeof(CKWORD) * indexCount, false);
        *startIndex = ibo->m_CurrentICount;
        ibo->m_CurrentICount += indexCount;
    }
    else
    {
        pdata = ibo->Lock(this, 0, sizeof(CKWORD) * indexCount, true);
        *startIndex = 0;
        ibo->m_CurrentICount = indexCount;
    }
    if (pdata)
        std::memcpy(pdata, indices, sizeof(CKWORD) * indexCount);
    ibo->Unlock(this);
    return ibo;
}

CKDX11IndexBufferDesc *CKDX11RasterizerContext::TriangleFanToList(CKWORD VOffset, CKDWORD VCount, int *startIndex, int* newIndexCount)
{
    ZoneScopedN(__FUNCTION__);
    static std::vector<CKWORD> strip_index;
    strip_index.clear();
    strip_index.reserve(VCount * 3);
    // Center at VOffset
    for (CKWORD i = 2; i < VCount; ++i)
    {
        strip_index.emplace_back(VOffset);
        strip_index.emplace_back(i - 1 + VOffset);
        strip_index.emplace_back(i + VOffset);
    }
    if (strip_index.empty())
        return nullptr;
    *newIndexCount = strip_index.size();
    return GenerateIB(strip_index.data(), strip_index.size(), startIndex);
}

CKDX11IndexBufferDesc *CKDX11RasterizerContext::TriangleFanToList(CKWORD *indices, int count, int *startIndex,
                                                                  int *newIndexCount)
{
    ZoneScopedN(__FUNCTION__);
    if (!indices)
        return nullptr;
    std::vector<CKWORD> strip_index;
    CKWORD center = indices[0];
    for (CKWORD i = 2; i < count; ++i)
    {
        strip_index.emplace_back(center);
        strip_index.emplace_back(indices[i - 1]);
        strip_index.emplace_back(indices[i]);
    }
    if (strip_index.empty())
        return nullptr;
    *newIndexCount = strip_index.size();
    return GenerateIB(strip_index.data(), strip_index.size(), startIndex);
}

CKBOOL CKDX11RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount,
                                              VxDrawPrimitiveData *data)
{
#if (LOG_IA) || (STATUS)
    ++directbat;
#endif
    ZoneScopedN(__FUNCTION__);
    TracyD3D11Zone(g_D3d11Ctx, __FUNCTION__);
    if (!m_SceneBegined)
        BeginScene();
    CKBOOL clip = 0;
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    if ((data->Flags & CKRST_DP_DOCLIP))
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 1);
        clip = 1;
    }
    else
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 0);
    }
    
    CKDWORD VB = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    auto *vbo = static_cast<CKDX11VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (vbo && (vbo->m_MaxVertexCount < data->VertexCount || vertexSize != vbo->m_VertexSize))
    {
        delete vbo;
        m_VertexBuffers[VB] = nullptr;
        vbo = nullptr;
    }
    if (!vbo)
    {
        vbo = new CKDX11VertexBufferDesc;
        vbo->m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        vbo->m_VertexFormat = vertexFormat;
        vbo->m_VertexSize = vertexSize;
#if VB_STRICT == 1
        vbo->m_MaxVertexCount = data->VertexCount;
#else
        vbo->m_MaxVertexCount = (data->VertexCount + 100 > DEFAULT_VB_SIZE) ? data->VertexCount + 100 : DEFAULT_VB_SIZE;
#endif
        vbo->m_CurrentVCount = 0;
        vbo->Create(this);
    }
    m_VertexBuffers[VB] = vbo;

    assert(vertexSize == vbo->m_VertexSize);

    void *pbData = nullptr;
    CKDWORD vbase = 0;
    if (vbo->m_CurrentVCount + data->VertexCount <= vbo->m_MaxVertexCount)
    {
        TracyPlot("Lock offset", (int64_t)vertexSize * vbo->m_CurrentVCount);
        TracyPlot("Lock len", (int64_t)vertexSize * data->VertexCount);
        pbData = vbo->Lock(this, vertexSize * vbo->m_CurrentVCount, vertexSize * data->VertexCount, false);
        vbase = vbo->m_CurrentVCount;
        vbo->m_CurrentVCount += data->VertexCount;
    }
    else
    {
        TracyPlot("Lock offset", 0ll);
        TracyPlot("Lock len", (int64_t)vertexSize * data->VertexCount);
        pbData = vbo->Lock(this, 0, vertexSize * data->VertexCount, true);
        vbo->m_CurrentVCount = data->VertexCount;
    }
    {
        ZoneScopedN("CKRSTLoadVertexBuffer");
        CKRSTLoadVertexBuffer(static_cast<CKBYTE *>(pbData), vertexFormat, vertexSize, data);
    }
    vbo->Unlock(this);
    return InternalDrawPrimitive(pType, vbo, vbase, data->VertexCount, indices, indexcount);
}

CKBOOL CKDX11RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD StartVIndex,
                                                CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if (LOG_IA) || (STATUS)
    ++vbbat;
#endif
    ZoneScopedN(__FUNCTION__);
    TracyD3D11Zone(g_D3d11Ctx, __FUNCTION__);
    if (VB >= m_VertexBuffers.Size())
        return FALSE;
    CKVertexBufferDesc *vbo = m_VertexBuffers[VB];
    if (!vbo)
        return FALSE;
    if (!m_SceneBegined)
        BeginScene();
    return InternalDrawPrimitive(pType, static_cast<CKDX11VertexBufferDesc *>(vbo), StartVIndex,
                                   VertexCount, indices, indexcount);
}

CKBOOL CKDX11RasterizerContext::InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKDX11VertexBufferDesc *vbo,
                                                        CKDWORD StartVertex, CKDWORD VertexCount, CKWORD *indices,
                                                        int indexcount)
{
    ZoneScopedN(__FUNCTION__);
    int ibbasecnt = 0;
    CKDX11IndexBufferDesc *ibo = nullptr;
    int newIndexCount;
    if (indices)
    {
        if (pType == VX_TRIANGLEFAN)
            ibo = TriangleFanToList(indices, indexcount, &ibbasecnt, &newIndexCount);
        else
            ibo = GenerateIB(indices, indexcount, &ibbasecnt);
    }
    else if (pType == VX_TRIANGLEFAN)
    {
        ibo = TriangleFanToList(0, VertexCount, &ibbasecnt, &newIndexCount);
    }

    if (pType == VX_TRIANGLEFAN)
        indexcount = newIndexCount;

    auto succeeded = AssemblyInput(vbo, ibo, pType);
    assert(succeeded);

    if (ibo)
    {
        m_DeviceContext->DrawIndexed(indexcount, ibbasecnt, StartVertex);

    } else
        m_DeviceContext->Draw(VertexCount, StartVertex);
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                                  CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if (LOG_IA) || (STATUS)
    ++vbibbat;
#endif

    // assert(pType != VX_TRIANGLEFAN);
    ZoneScopedN(__FUNCTION__);
    TracyD3D11Zone(g_D3d11Ctx, __FUNCTION__);

    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    auto *vbo = static_cast<CKDX11VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vbo)
        return FALSE;

    auto *ibo = static_cast<CKDX11IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ibo)
        return FALSE;

    auto succeeded = AssemblyInput(vbo, ibo, pType);
    assert(succeeded);

#if LOG_IA
    fprintf(stderr, "IA: VB offset: %d, vsize: %d\n", MinVIndex, vbo->m_VertexSize);
    fprintf(stderr, "IA: IB offset: %d\n", StartIndex);
#endif
    assert(VertexCount <= vbo->m_MaxVertexCount - MinVIndex); // check if vb is big enough
    assert(Indexcount <= ibo->m_MaxIndexCount - StartIndex); // check if ib is big enough
    m_DeviceContext->DrawIndexed(Indexcount, StartIndex, MinVIndex);
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    int result;

    if (ObjIndex >= m_Textures.Size())
        return FALSE;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
            result = CreateSpriteNPOT(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXBUFFER:
            result = CreateVertexBuffer(ObjIndex, static_cast<CKVertexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_INDEXBUFFER:
            result = CreateIndexBuffer(ObjIndex, static_cast<CKIndexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXSHADER:
            result = CreateVertexShader(ObjIndex, static_cast<CKVertexShaderDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_PIXELSHADER:
            result = CreatePixelShader(ObjIndex, static_cast<CKPixelShaderDesc *>(DesiredFormat));
            break;
        default:
            return 0;
    }
    return result;
}

CKBOOL CKDX11RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKDX11TextureDesc *desc = static_cast<CKDX11TextureDesc *>(m_Textures[Texture]);
    if (!desc)
        return FALSE;
    VxImageDescEx dst;
    dst.Size = sizeof(VxImageDescEx);
    ZeroMemory(&dst.Flags, sizeof(VxImageDescEx) - sizeof(dst.Size));
    dst.Width = SurfDesc.Width;
    dst.Height = SurfDesc.Height;
    dst.BitsPerPixel = 32;
    dst.BytesPerLine = 4 * SurfDesc.Width;
    dst.AlphaMask = 0xFF000000;
    dst.RedMask = 0x0000FF;
    dst.GreenMask = 0x00FF00;
    dst.BlueMask = 0xFF0000;
    dst.Image = new uint8_t[dst.Width * dst.Height * (dst.BitsPerPixel / 8)];
    VxDoBlit(SurfDesc, dst);
    if (!(SurfDesc.AlphaMask || SurfDesc.Flags >= _DXT1))
        VxDoAlphaBlit(dst, 255);
    CKBOOL ret = desc->Create(this, dst.Image);
    delete dst.Image;
    return ret;
}

CKBOOL CKDX11RasterizerContext::LoadSprite(CKDWORD Sprite, const VxImageDescEx &SurfDesc)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    LoadTexture(spr->Textures.Front().IndexTexture, SurfDesc);
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}
CKBOOL CKDX11RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face);
}
CKBOOL CKDX11RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    VxDrawPrimitiveData pd{};
    pd.VertexCount = 4;
    pd.Flags = CKRST_DP_VCT;
    VxVector4 p[4] = {VxVector4(dst->left, dst->top, 0, 1.), VxVector4(dst->right, dst->top, 0, 1.),
                      VxVector4(dst->right, dst->bottom, 0, 1.), VxVector4(dst->left, dst->bottom, 0, 1.)};
    CKDWORD c[4] = {~0U, ~0U, ~0U, ~0U};
    Vx2DVector t[4] = {Vx2DVector(src->left, src->top), Vx2DVector(src->right, src->top),
                       Vx2DVector(src->right, src->bottom), Vx2DVector(src->left, src->bottom)};
    for (int i = 0; i < 4; ++i)
    {
        t[i].x /= spr->Format.Width;
        t[i].y /= spr->Format.Height;
    }
    pd.PositionStride = sizeof(VxVector4);
    pd.ColorStride = sizeof(CKDWORD);
    pd.TexCoordStride = sizeof(Vx2DVector);
    pd.PositionPtr = p;
    pd.ColorPtr = c;
    pd.TexCoordPtr = t;
    CKWORD idx[6] = {0, 1, 2, 0, 2, 3};
    SetTexture(spr->Textures.Front().IndexTexture);
    SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    DrawPrimitive(VX_TRIANGLELIST, idx, 6, &pd);
    return TRUE;
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

void *CKDX11RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
                                                CKRST_LOCKFLAGS Lock)
{
    if (VB > m_VertexBuffers.Size())
        return nullptr;
    auto *desc = static_cast<CKDX11VertexBufferDesc *>(m_VertexBuffers[VB]);

    assert(StartVertex + VertexCount <= desc->m_MaxVertexCount);
    return desc->Lock(this, StartVertex, VertexCount * desc->m_VertexSize, (Lock & CKRST_LOCK_DISCARD));
}
CKBOOL CKDX11RasterizerContext::UnlockVertexBuffer(CKDWORD VB) {
    if (VB > m_VertexBuffers.Size())
        return FALSE;
    auto *desc = static_cast<CKDX11VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!desc)
        return FALSE;
    desc->Unlock(this);
    return TRUE;
}

void *CKDX11RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    if (IB > m_IndexBuffers.Size())
        return nullptr;
    auto *desc = static_cast<CKDX11IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!desc)
        return nullptr;
    return desc->Lock(this, StartIndex * sizeof(CKWORD), IndexCount * sizeof(CKWORD), Lock & CKRST_LOCK_DISCARD);
}
CKBOOL CKDX11RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB > m_IndexBuffers.Size())
        return FALSE;
    auto *desc = static_cast<CKDX11IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!desc)
        return FALSE;
    desc->Unlock(this);
    return TRUE;
}
CKBOOL CKDX11RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat) {
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    if (m_Textures[Texture])
        return TRUE;
#if LOG_CREATETEXTURE
    fprintf(stderr, "create texture %d %dx%d %x\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height,
            DesiredFormat->Flags);
#endif
    CKDX11TextureDesc *desc = new CKDX11TextureDesc(DesiredFormat);
    m_Textures[Texture] = desc;
    return TRUE;
}

CKBOOL CKDX11RasterizerContext::CreateSpriteNPOT(CKDWORD Sprite, CKSpriteDesc *DesiredFormat)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size() || !DesiredFormat)
        return FALSE;
    if (m_Sprites[Sprite])
        delete m_Sprites[Sprite];
    m_Sprites[Sprite] = new CKSpriteDesc();
    CKSpriteDesc *spr = m_Sprites[Sprite];
    spr->Flags = DesiredFormat->Flags;
    spr->Format = DesiredFormat->Format;
    spr->MipMapCount = DesiredFormat->MipMapCount;
    spr->Owner = m_Driver->m_Owner;
    CKSPRTextInfo ti;
    ti.IndexTexture = m_Driver->m_Owner->CreateObjectIndex(CKRST_OBJ_TEXTURE);
    CreateObject(ti.IndexTexture, CKRST_OBJ_TEXTURE, DesiredFormat);
    spr->Textures.PushBack(ti);
    return TRUE;
}

bool operator==(const CKVertexShaderDesc &a, const CKVertexShaderDesc& b)
{
    return a.m_FunctionSize == b.m_FunctionSize &&
        (a.m_Function == b.m_Function || 
            memcmp(a.m_Function, b.m_Function, a.m_FunctionSize) == 0);
}

CKBOOL CKDX11RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat) { 
    if (VShader >= m_VertexShaders.Size() || !DesiredFormat)
        return FALSE;
    auto *desc = m_VertexShaders[VShader];
    CKDX11VertexShaderDesc *d11desc = nullptr;

    if (!desc || *DesiredFormat == *desc)
    {
        d11desc = dynamic_cast<CKDX11VertexShaderDesc *>(desc); // Check if object got from array is actually valid
        if (d11desc && d11desc->DxBlob) // A valid, while identical object already exists
            return TRUE;
    }
    delete desc;
    m_VertexShaders[VShader] = nullptr;
    d11desc = new CKDX11VertexShaderDesc;
    auto *d11fmt = dynamic_cast<CKDX11VertexShaderDesc *>(DesiredFormat);
    if (d11fmt)
        *d11desc = *d11fmt;
    else {
        d11desc->m_Function = DesiredFormat->m_Function;
        d11desc->m_FunctionSize = DesiredFormat->m_FunctionSize;
    }
    CKBOOL succeeded = d11desc->Create(this);
    if (succeeded)
        m_VertexShaders[VShader] = d11desc;
    return succeeded;
}

bool operator==(const CKPixelShaderDesc& a, const CKPixelShaderDesc& b)
{
    return a.m_FunctionSize == b.m_FunctionSize &&
        (a.m_Function == b.m_Function || memcmp(a.m_Function, b.m_Function, a.m_FunctionSize) == 0);
}

CKBOOL CKDX11RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat) {
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return FALSE;
    auto *desc = m_PixelShaders[PShader];
    CKDX11PixelShaderDesc *d11desc = nullptr;
    if (!desc || *DesiredFormat == *desc)
    {
        d11desc = dynamic_cast<CKDX11PixelShaderDesc *>(desc); // Check if object got from array is actually valid
        if (d11desc && d11desc->DxBlob) // A valid, while identical object already exists
            return TRUE;
    }
    delete desc;
    m_PixelShaders[PShader] = nullptr;
    d11desc = new CKDX11PixelShaderDesc;
    d11desc->m_Function = DesiredFormat->m_Function;
    d11desc->m_FunctionSize = DesiredFormat->m_FunctionSize;
    CKBOOL succeeded = d11desc->Create(this);
    if (succeeded)
        m_PixelShaders[PShader] = d11desc;
    return succeeded;
}

bool operator==(const CKVertexBufferDesc& a, const CKVertexBufferDesc& b){
    return a.m_CurrentVCount == b.m_CurrentVCount && a.m_Flags == b.m_Flags &&
        a.m_MaxVertexCount == b.m_MaxVertexCount && a.m_VertexFormat == b.m_VertexFormat &&
        a.m_VertexSize == b.m_VertexSize;
}

CKBOOL CKDX11RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return FALSE;
    
    auto *vbo = m_VertexBuffers[VB];
    CKDX11VertexBufferDesc *dx11vb = nullptr;
    if (vbo && *DesiredFormat == *vbo)
    {
        dx11vb = dynamic_cast<CKDX11VertexBufferDesc *>(vbo);
        if (dx11vb && dx11vb->DxBuffer)
            return TRUE;
    }
    delete vbo;
    m_VertexBuffers[VB] = nullptr;
    
    dx11vb = new CKDX11VertexBufferDesc;
    dx11vb->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    dx11vb->m_Flags = DesiredFormat->m_Flags;
    dx11vb->m_MaxVertexCount =
#if VB_STRICT == 1
        DesiredFormat->m_MaxVertexCount;
#else
        (DesiredFormat->m_MaxVertexCount + 100 > DEFAULT_VB_SIZE)
        ? DesiredFormat->m_MaxVertexCount + 100
        : DEFAULT_VB_SIZE;
#endif
    dx11vb->m_VertexFormat = DesiredFormat->m_VertexFormat;
    dx11vb->m_VertexSize = FVF::ComputeVertexSize(DesiredFormat->m_VertexFormat);

    CKBOOL succeeded = dx11vb->Create(this);
    if (succeeded)
        m_VertexBuffers[VB] = dx11vb;
    return succeeded;
}

bool operator==(const CKIndexBufferDesc &a, const CKIndexBufferDesc &b)
{
    return a.m_Flags == b.m_Flags && a.m_CurrentICount == b.m_CurrentICount &&
        a.m_MaxIndexCount == b.m_MaxIndexCount;
}

CKBOOL CKDX11RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat) {
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;

    auto *vbDesc = m_IndexBuffers[IB];
    CKDX11IndexBufferDesc *dx11ib = nullptr;
    if (!vbDesc || *DesiredFormat == *vbDesc)
    {
        dx11ib = dynamic_cast<CKDX11IndexBufferDesc *>(vbDesc);
        if (dx11ib && dx11ib->DxBuffer)
            return TRUE;
    }
    delete vbDesc;
    m_IndexBuffers[IB] = nullptr;

    dx11ib = new CKDX11IndexBufferDesc;
    dx11ib->m_CurrentICount = DesiredFormat->m_CurrentICount;
    dx11ib->m_Flags = DesiredFormat->m_Flags;
    dx11ib->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount;
    dx11ib->m_Flags = DesiredFormat->m_Flags;

    CKBOOL succeeded = dx11ib->Create(this);
    if (succeeded)
        m_IndexBuffers[IB] = dx11ib;
    return succeeded;
}

CKBOOL CKDX11RasterizerContext::AssemblyInput(CKDX11VertexBufferDesc *vbo, CKDX11IndexBufferDesc *ibo,
                                              VXPRIMITIVETYPE pType)
{
    ZoneScopedN(__FUNCTION__);
    if (!vbo)
        return FALSE;

    HRESULT hr;
    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    switch (pType & 0xf)
    {
        case VX_LINELIST:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case VX_LINESTRIP:
            topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case VX_TRIANGLELIST:
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case VX_TRIANGLESTRIP:
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case VX_TRIANGLEFAN:
            // D3D11 does not support triangle fan, turn it into triangle list
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        default:
            break;
    }
    m_DeviceContext->IASetPrimitiveTopology(topology);
    vbo->Bind(this);
    if (ibo)
        ibo->Bind(this);

    CKDWORD VShader;
#if defined(DEBUG) || defined(_DEBUG)
    try
    {
#endif
        VShader = m_VertexShaderMap.at(vbo->m_VertexFormat);
#if defined(DEBUG) || defined(_DEBUG)
    }
    catch (...)
    {
#if LOG_IA
        fprintf(stderr, "FVF: 0x%x\n", vbo->m_VertexFormat);
#endif
        assert(false);
        return FALSE;
    }
#endif
    auto *vs = static_cast<CKDX11VertexShaderDesc *>(m_VertexShaders[VShader]);
    vs->Bind(this);
    if (vbo->m_VertexFormat != m_VSCBuffer.FVF)
    {
        m_VSConstantBufferUpToDate = FALSE;
        m_VSCBuffer.FVF = vbo->m_VertexFormat;
        m_PSConstantBufferUpToDate = FALSE;
        m_PSCBuffer.FVF = vbo->m_VertexFormat;
        if (m_InputLayoutMap.find(vbo->m_VertexFormat) == m_InputLayoutMap.end())
        {
            if (!FVF::CreateInputLayoutFromFVF(vbo->m_VertexFormat, m_InputElementMap[vbo->m_VertexFormat]))
                return FALSE;
            D3DCall(m_Device->CreateInputLayout(m_InputElementMap[vbo->m_VertexFormat].data(),
                                                m_InputElementMap[vbo->m_VertexFormat].size(), 
                vs->m_Function, vs->m_FunctionSize, m_InputLayoutMap[vbo->m_VertexFormat].GetAddressOf()))
        }
        m_DeviceContext->IASetInputLayout(m_InputLayoutMap[vbo->m_VertexFormat].Get());
    }

    if (!m_VSConstantBufferUpToDate)
    {
        ZoneScopedN("VSConstantBuffer Update");
        UpdateMatrices(WORLD_TRANSFORM);
        // this->UpdateMatrices(VIEW_TRANSFORM);
        Vx3DTransposeMatrix(m_VSCBuffer.WorldMatrix, m_WorldMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ViewMatrix, m_ViewMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ProjectionMatrix, m_ProjectionMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.TotalMatrix, m_TotalMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldMatrix, m_WorldMatrix);
        // VxMatrix mat;
        // Vx3DTransposeMatrix(mat, m_ModelViewMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldViewMatrix, m_ModelViewMatrix);
        // Vx3DTransposeMatrix(m_VSCBuffer.ViewportMatrix, m_VSCBuffer.ViewportMatrix);
        D3D11_MAPPED_SUBRESOURCE ms;
        D3DCall(m_DeviceContext->Map(m_VSConstantBuffer.DxBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms));
        std::memcpy(ms.pData, &m_VSCBuffer, sizeof(VSConstantBufferStruct));
        m_DeviceContext->Unmap(m_VSConstantBuffer.DxBuffer.Get(), NULL);

        m_DeviceContext->VSSetConstantBuffers(0, 1, m_VSConstantBuffer.DxBuffer.GetAddressOf());
        m_VSConstantBufferUpToDate = TRUE;
    }
     
    if (!m_PSConstantBufferUpToDate)
    {
#if LOGGING && (LOG_ALPHAFLAG)
        fprintf(stderr, "IA: Alpha flag, thr: 0x%x, %.2f\n", m_PSCBuffer.AlphaFlags, m_PSCBuffer.AlphaThreshold);
#endif
        ZoneScopedN("PSConstantBuffer Update");
        VxMatrix mat;
        Vx3DInverseMatrix(mat, m_ViewMatrix);
        m_PSCBuffer.ViewPosition = VxVector(mat[3][0], mat[3][1], mat[3][2]);
        D3D11_MAPPED_SUBRESOURCE ms;
        D3DCall(m_DeviceContext->Map(m_PSConstantBuffer.DxBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms));
        std::memcpy(ms.pData, &m_PSCBuffer, sizeof(PSConstantBufferStruct));
        m_DeviceContext->Unmap(m_PSConstantBuffer.DxBuffer.Get(), NULL);
        m_DeviceContext->PSSetConstantBuffers(0, 1, m_PSConstantBuffer.DxBuffer.GetAddressOf());
        m_PSConstantBufferUpToDate = TRUE;
    }

    if (!m_PSLightConstantBufferUpToDate)
    {
        ZoneScopedN("PSLightConstantBuffer Update");
        D3D11_MAPPED_SUBRESOURCE ms;
        D3DCall(m_DeviceContext->Map(m_PSLightConstantBuffer.DxBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms));
        std::memcpy(ms.pData, &m_PSLightCBuffer, sizeof(PSLightConstantBufferStruct));
        m_DeviceContext->Unmap(m_PSLightConstantBuffer.DxBuffer.Get(), NULL);
        m_DeviceContext->PSSetConstantBuffers(1, 1, m_PSLightConstantBuffer.DxBuffer.GetAddressOf());
        m_PSLightConstantBufferUpToDate = TRUE;
    }

    if (!m_PSTexCombinatorConstantBufferUpToDate)
    {
        ZoneScopedN("PSTexCombinatorConstantBuffer Update");
        D3D11_MAPPED_SUBRESOURCE ms;
        D3DCall(m_DeviceContext->Map(m_PSTexCombinatorConstantBuffer.DxBuffer.Get(), NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms));
        std::memcpy(ms.pData, &m_PSTexCombinatorCBuffer, sizeof(PSTexCombinatorConstantBufferStruct));
        m_DeviceContext->Unmap(m_PSTexCombinatorConstantBuffer.DxBuffer.Get(), NULL);
        m_DeviceContext->PSSetConstantBuffers(2, 1, m_PSTexCombinatorConstantBuffer.DxBuffer.GetAddressOf());
        m_PSTexCombinatorConstantBufferUpToDate = TRUE;
    }
#if LOGGING && LOG_IA
    fprintf(stderr, "IA: vs %s\n", vs->DxEntryPoint);
#endif

    m_VSCBuffer.FVF = vbo->m_VertexFormat;

    if (!m_RasterizerStateUpToDate)
    {
        m_DeviceContext->RSSetState(m_RasterizerState.Get());
        m_RasterizerStateUpToDate = TRUE;
    }
    if (!m_BlendStateUpToDate)
    {
        D3DCall(m_Device->CreateBlendState(&m_BlendStateDesc, m_BlendState.ReleaseAndGetAddressOf()));
        m_DeviceContext->OMSetBlendState(m_BlendState.Get(), NULL, 0xFFFFFF);
        m_BlendStateUpToDate = TRUE;
    }
    if (!m_DepthStencilStateUpToDate)
    {
        D3DCall(m_Device->CreateDepthStencilState(&m_DepthStencilDesc, m_DepthStencilState.ReleaseAndGetAddressOf()));
        m_DeviceContext->OMSetDepthStencilState(m_DepthStencilState.Get(), 1);
        m_DepthStencilStateUpToDate = TRUE;
    }

    for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        if (!m_SamplerStateUpToDate[i])
        {
            if (m_Filter[i].modified_)
            {
                m_SamplerDesc[i].Filter = m_Filter[i].GetFilterMode(true);
            }
            D3DCall(m_Device->CreateSamplerState(&m_SamplerDesc[i], m_SamplerState[i].ReleaseAndGetAddressOf()));
            m_SamplerStateUpToDate[i] = TRUE;
        }
        m_SamplerRaw[i] = m_SamplerState[i].Get();
    }
    m_DeviceContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, m_SamplerRaw);
    
    return TRUE;
}
