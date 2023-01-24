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

//-------------------------------------------------------------------------------------
// Returns bits-per-pixel for a given DXGI format, or 0 on failure
//-------------------------------------------------------------------------------------
size_t BitsPerPixel(DXGI_FORMAT fmt)
{
    switch (static_cast<int>(fmt))
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_YUY2:
            return 32;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            return 24;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 16;

        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_NV11:
            return 12;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
            return 8;

        case DXGI_FORMAT_R1_UNORM:
            return 1;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 4;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 8;

        default:
            return 0;
    }
}

CKBOOL CKDX11RasterizerDriver::InitializeCaps(Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter,
                                              Microsoft::WRL::ComPtr<IDXGIOutput> Output)
{
    HRESULT hr;

    m_Adapter = Adapter;
    m_Output = Output;

    // pretend we have a 640x480 @ 16bpp mode for compatibility reasons
    VxDisplayMode mode {640, 480, 16, 60};
    m_DisplayModes.PushBack(mode);

    UINT numModes = 0;
    DXGI_MODE_DESC *displayModes = NULL;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // Get the number of elements
    D3DCall(m_Output->GetDisplayModeList(format, 0, &numModes, NULL));

    displayModes = new DXGI_MODE_DESC[numModes];

    // Get the list
    D3DCall(m_Output->GetDisplayModeList(format, 0, &numModes, displayModes));
    for (UINT i = 0; i < numModes; ++i)
    {
        auto &dm = displayModes[i];
        mode.Width = dm.Width;
        mode.Height = dm.Height;
        mode.Bpp = BitsPerPixel(dm.Format);
        mode.RefreshRate = (int)((float)dm.RefreshRate.Numerator / (float)dm.RefreshRate.Denominator);
        m_DisplayModes.PushBack(mode);
    }
    delete[] displayModes;
    m_Hardware = 1;
    // shameless copy-pasting...
    ZeroMemory(&m_3DCaps, sizeof(m_3DCaps));
    ZeroMemory(&m_2DCaps, sizeof(m_2DCaps));
    m_3DCaps.CKRasterizerSpecificCaps = 
        (CKRST_SPECIFICCAPS_SPRITEASTEXTURES |
            CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER | 
            CKRST_SPECIFICCAPS_CANDOINDEXBUFFER | 
            CKRST_SPECIFICCAPS_GLATTENUATIONMODEL | 
            CKRST_SPECIFICCAPS_HARDWARETL | 
            CKRST_SPECIFICCAPS_DX8);
    m_3DCaps.MaxNumberTextureStage = 8; //?
    m_3DCaps.MaxNumberBlendStage = 8; // fake it until we make it
    m_3DCaps.MaxActiveLights = 16; // this is a lie (at least for now)
    m_3DCaps.MinTextureWidth = 1; // we are using texture of width 1 for blank textures
    m_3DCaps.MinTextureHeight = 1; // so we know it must work... or do we?
    m_3DCaps.MaxTextureWidth = 1024; // we know OpenGL guarantees this to be at least 1024...
    m_3DCaps.MaxTextureHeight = 1024; // and I'm too lazy to create a context here...
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_DIRECTIONALLIGHTS;
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_TEXGEN;
    m_3DCaps.AlphaCmpCaps = 0xff; // we have TECHNOLOGY
    m_3DCaps.ZCmpCaps = 0xff; // who wouldn't be 0xff here?
    m_3DCaps.TextureAddressCaps = 0x1f; // everything
    m_3DCaps.TextureCaps |= CKRST_TEXTURECAPS_PERSPECTIVE; // not only do we support it, it's ALWAYS on
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_MASKZ; // glDepthMask
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CONFORMANT;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLNONE;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCW;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCCW;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_ZTEST;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_FOGPIXEL; // vertex fog? what's that paleocene technology?
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WBUFFER;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WFOG; // w fog hardcoded in shader
    m_3DCaps.SrcBlendCaps = 0x1fff; // everything
    m_3DCaps.DestBlendCaps = 0x1fff; // ditto
    m_2DCaps.AvailableVideoMemory = 1024;
    m_2DCaps.MaxVideoMemory = 1024;
    m_2DCaps.Family = CKRST_DIRECTX;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_WINDOWED);

    D3DCall(m_Adapter->GetDesc1(&m_AdapterDesc));
    D3DCall(m_Output->GetDesc(&m_OutputDesc));
    m_Desc = "";
    char buf[128];
    ZeroMemory(buf, 128);
    int len = WideCharToMultiByte(CP_ACP, 0, m_OutputDesc.DeviceName, wcslen(m_OutputDesc.DeviceName), nullptr, 0,
                        nullptr, nullptr);
    WideCharToMultiByte(CP_ACP, 0, m_OutputDesc.DeviceName, wcslen(m_OutputDesc.DeviceName), buf, len,
                                  nullptr, nullptr);
    m_Desc << buf << " @ ";
    WideCharToMultiByte(CP_ACP, 0, m_AdapterDesc.Description, wcslen(m_AdapterDesc.Description),
                        buf, 128, nullptr, nullptr);
    m_Desc << buf << " (DX11, DXGI 1.1)";
    m_CapsUpToDate = TRUE;
    return TRUE;
}
