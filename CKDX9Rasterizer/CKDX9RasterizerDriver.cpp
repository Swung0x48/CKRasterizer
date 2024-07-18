#include "CKDX9Rasterizer.h"

#include <intrin.h>
#include <CKContext.h>

static const D3DFORMAT TextureFormats[] = {
    D3DFMT_R8G8B8,
    D3DFMT_A8R8G8B8,
    D3DFMT_X8R8G8B8,
    D3DFMT_R5G6B5,
    D3DFMT_X1R5G5B5,
    D3DFMT_A1R5G5B5,
    D3DFMT_A4R4G4B4,
    D3DFMT_R3G3B2,
    D3DFMT_A8,
    D3DFMT_A8R3G3B2,
    D3DFMT_X4R4G4B4,
    D3DFMT_A2B10G10R10,
    D3DFMT_A8B8G8R8,
    D3DFMT_X8B8G8R8,
    D3DFMT_G16R16,
    D3DFMT_A2R10G10B10,
    D3DFMT_A16B16G16R16,

    D3DFMT_A8P8,
    D3DFMT_P8,

    D3DFMT_L8,
    D3DFMT_A8L8,
    D3DFMT_A4L4,

    D3DFMT_V8U8,
    D3DFMT_L6V5U5,
    D3DFMT_X8L8V8U8,
    D3DFMT_Q8W8V8U8,
    D3DFMT_V16U16,
    D3DFMT_A2W10V10U10,

    D3DFMT_UYVY,
    D3DFMT_R8G8_B8G8,
    D3DFMT_YUY2,
    D3DFMT_G8R8_G8B8,
    D3DFMT_DXT1,
    D3DFMT_DXT2,
    D3DFMT_DXT3,
    D3DFMT_DXT4,
    D3DFMT_DXT5
};

CKDX9RasterizerDriver::CKDX9RasterizerDriver(CKDX9Rasterizer *rst) { m_Owner = rst; }

CKDX9RasterizerDriver::~CKDX9RasterizerDriver() {}

CKRasterizerContext *CKDX9RasterizerDriver::CreateContext()
{
    CKDX9RasterizerContext *context = new CKDX9RasterizerContext();
    context->m_Driver = this;
    context->m_Owner = static_cast<CKDX9Rasterizer *>(m_Owner);
    m_Contexts.PushBack(context);
    return context;
}

CKBOOL CKDX9RasterizerDriver::InitializeCaps(int AdapterIndex, D3DDEVTYPE DevType)
{
    m_AdapterIndex = AdapterIndex;
    m_Inited = TRUE;

    IDirect3D9Ex *pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;

    if (FAILED(pD3D->GetAdapterIdentifier(AdapterIndex, D3DENUM_WHQL_LEVEL, &m_D3DIdentifier)))
        return FALSE;

    D3DDISPLAYMODE displayMode;
    if (FAILED(pD3D->GetAdapterDisplayMode(AdapterIndex, &displayMode)))
        return FALSE;

    if (FAILED(pD3D->GetDeviceCaps(AdapterIndex, D3DDEVTYPE_HAL, &m_D3DCaps)))
        return FALSE;

    const D3DFORMAT allowedAdapterFormatArray[] = {
       D3DFMT_A1R5G5B5,
       D3DFMT_A2R10G10B10,
       D3DFMT_A8R8G8B8,
       D3DFMT_R5G6B5,
       D3DFMT_X1R5G5B5,
       D3DFMT_X8R8G8B8
    };
    const int allowedAdapterFormatArrayCount = sizeof(allowedAdapterFormatArray) / sizeof(allowedAdapterFormatArray[0]);

    for (int i = 0; i < allowedAdapterFormatArrayCount; i++)
    {
        D3DFORMAT allowedAdapterFormat = allowedAdapterFormatArray[i];
        UINT numAdapterModes = pD3D->GetAdapterModeCount(AdapterIndex, allowedAdapterFormat);

        for (UINT mode = 0; mode < numAdapterModes; mode++)
        {
            D3DDISPLAYMODE displayMode;
            pD3D->EnumAdapterModes(AdapterIndex, allowedAdapterFormat, mode, &displayMode);
            if (displayMode.Width >= 640 && displayMode.Height >= 400)
            {
                if (SUCCEEDED(pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, displayMode.Format, displayMode.Format, FALSE)))
                {
                    if (!m_RenderFormats.IsHere(displayMode.Format))
                        m_RenderFormats.PushBack(displayMode.Format);
                }

                VX_PIXELFORMAT pf = D3DFormatToVxPixelFormat(displayMode.Format);
                VxImageDescEx desc;
                VxPixelFormat2ImageDesc(pf, desc);
                VxDisplayMode dm = {
                    (int)displayMode.Width,
                    (int)displayMode.Height,
                    desc.BitsPerPixel,
                    (int)displayMode.RefreshRate
                };

                if (!m_DisplayModes.IsHere(dm))
                    m_DisplayModes.PushBack(dm);
            }
        }
    }

    const D3DFORMAT textureFormatArray[] = {
        D3DFMT_R8G8B8,
        D3DFMT_A8R8G8B8,
        D3DFMT_X8R8G8B8,
        D3DFMT_R5G6B5,
        D3DFMT_X1R5G5B5,
        D3DFMT_A1R5G5B5,
        D3DFMT_A4R4G4B4,
        D3DFMT_R3G3B2,
        D3DFMT_DXT1,
        D3DFMT_DXT3,
        D3DFMT_DXT5,
        D3DFMT_V8U8,
        D3DFMT_L6V5U5,
        D3DFMT_X8L8V8U8,
        D3DFMT_V16U16
    };
    const int textureFormatArrayCount = sizeof(textureFormatArray) / sizeof(textureFormatArray[0]);

    for (int i = 0; i < textureFormatArrayCount; i++)
    {
        D3DFORMAT format = textureFormatArray[i];
        if (CheckDeviceFormat(format, m_RenderFormats.Front()))
        {
            CKTextureDesc desc;
            D3DFormatToTextureDesc(format, &desc);
            m_TextureFormats.PushBack(desc);
        }
    }

    m_Hardware = TRUE;
    m_3DCaps.StencilCaps = m_D3DCaps.StencilCaps;
    m_3DCaps.DevCaps = m_D3DCaps.DevCaps;
    m_3DCaps.MinTextureWidth = 1;
    m_3DCaps.MinTextureHeight = 1;
    m_3DCaps.MaxTextureWidth = m_D3DCaps.MaxTextureWidth;
    m_3DCaps.MaxTextureHeight = m_D3DCaps.MaxTextureHeight;
    m_3DCaps.MaxTextureRatio = m_D3DCaps.MaxTextureAspectRatio;
    if (m_D3DCaps.MaxTextureAspectRatio == 0)
    {
        m_3DCaps.MaxTextureRatio = m_D3DCaps.MaxTextureWidth;
    }
    m_3DCaps.VertexCaps = m_D3DCaps.VertexProcessingCaps;
    m_3DCaps.MaxClipPlanes = m_D3DCaps.MaxUserClipPlanes;
    m_3DCaps.MaxNumberBlendStage = m_D3DCaps.MaxTextureBlendStages;
    m_3DCaps.MaxActiveLights = m_D3DCaps.MaxActiveLights;
    m_3DCaps.MaxNumberTextureStage = m_D3DCaps.MaxSimultaneousTextures;
    DWORD TextureFilterCaps = m_D3DCaps.TextureFilterCaps;
    if ((TextureFilterCaps & (D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MINFPOINT)) != 0)
        m_3DCaps.TextureFilterCaps = TextureFilterCaps | CKRST_TFILTERCAPS_NEAREST;
    if ((TextureFilterCaps & (D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MINFLINEAR)) != 0)
        m_3DCaps.TextureFilterCaps |= CKRST_TFILTERCAPS_LINEAR;
    if ((TextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT) != 0)
        m_3DCaps.TextureFilterCaps |= (CKRST_TFILTERCAPS_LINEARMIPNEAREST | CKRST_TFILTERCAPS_MIPNEAREST);
    if ((TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR) != 0)
        m_3DCaps.TextureFilterCaps |= (CKRST_TFILTERCAPS_LINEARMIPLINEAR | CKRST_TFILTERCAPS_MIPLINEAR);

    m_3DCaps.TextureCaps = m_D3DCaps.TextureCaps;
    m_3DCaps.TextureAddressCaps = m_D3DCaps.TextureAddressCaps;
    m_3DCaps.MiscCaps = m_D3DCaps.PrimitiveMiscCaps;
    m_3DCaps.ZCmpCaps = m_D3DCaps.ZCmpCaps;
    m_3DCaps.AlphaCmpCaps = m_D3DCaps.AlphaCmpCaps;
    m_3DCaps.DestBlendCaps = m_D3DCaps.DestBlendCaps;
    m_3DCaps.RasterCaps = m_D3DCaps.RasterCaps;
    m_3DCaps.SrcBlendCaps = m_D3DCaps.SrcBlendCaps;
    m_3DCaps.CKRasterizerSpecificCaps =
        CKRST_SPECIFICCAPS_SPRITEASTEXTURES |
        CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER |
        CKRST_SPECIFICCAPS_GLATTENUATIONMODEL |
        CKRST_SPECIFICCAPS_COPYTEXTURE |
        CKRST_SPECIFICCAPS_DX9 |
        CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;
    m_2DCaps.AvailableVideoMemory = 0;
    m_2DCaps.MaxVideoMemory = 0;
    m_2DCaps.Family = CKRST_DIRECTX;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);

    if (AdapterIndex == 0)
        m_2DCaps.Caps |= CKRST_2DCAPS_WINDOWED;

    HMONITOR hMonitor = pD3D->GetAdapterMonitor(AdapterIndex);
    MONITORINFOEXA Info;
    Info.cbSize = sizeof(MONITORINFOEXA);
    if (GetMonitorInfoA(hMonitor, &Info))
    {
        m_Desc = &Info.szDevice[4];
        m_Desc << " (" << (int)(Info.rcMonitor.right - Info.rcMonitor.left) << "x"
               << (int)(Info.rcMonitor.bottom - Info.rcMonitor.top) << ")";
        m_Desc << " @ " << m_D3DIdentifier.Description;
    }
    else
    {
        m_Desc = m_D3DIdentifier.Description;
    }
    XWORD pos = m_Desc.Find('\\');
    if (pos != XString::NOTFOUND)
        m_Desc = m_Desc.Crop(0, pos);

    if ((m_D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0)
    {
        m_IsHTL = TRUE;
        m_Desc << " (T&L DX9)";
        m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    }
    else
    {
        m_IsHTL = FALSE;
        m_Desc << " (Hardware DX9)";
        m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARE;
    }
    m_CapsUpToDate = TRUE;
    return TRUE;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestTextureFormat(CKTextureDesc *desc, D3DFORMAT AdapterFormat, DWORD Usage)
{
    IDirect3D9Ex *pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;

    if (!m_TextureFormats.Size())
    {
        for (auto format : TextureFormats)
            if (SUCCEEDED(pD3D->CheckDeviceFormat(m_AdapterIndex, D3DDEVTYPE_HAL, AdapterFormat, Usage, D3DRTYPE_TEXTURE, format)))
            {
                CKTextureDesc d;
                D3DFormatToTextureDesc(format, &d);
                m_TextureFormats.PushBack(d);
            }
    }
    auto origFormat = TextureDescToD3DFormat(desc);
    if (SUCCEEDED(pD3D->CheckDeviceFormat(m_AdapterIndex, D3DDEVTYPE_HAL, AdapterFormat, Usage, D3DRTYPE_TEXTURE, origFormat)))
        return origFormat;

    CKTextureDesc *best = NULL;
    unsigned int bestdiff = ~0U;
    for (auto i = m_TextureFormats.Begin(); i != m_TextureFormats.End(); ++i)
    {
        auto diff = (abs(i->Format.BitsPerPixel - desc->Format.BitsPerPixel) << 4) +
            __popcnt(i->Format.AlphaMask ^ desc->Format.AlphaMask) +
            __popcnt(i->Format.RedMask ^ desc->Format.RedMask) +
            __popcnt(i->Format.GreenMask ^ desc->Format.GreenMask) +
            __popcnt(i->Format.BlueMask ^ desc->Format.BlueMask);
        if (diff < bestdiff)
        {
            bestdiff = diff;
            best = i;
        }
    }
    if (best)
        return TextureDescToD3DFormat(best);
    return D3DFMT_UNKNOWN;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestRenderTargetFormat(int Bpp, CKBOOL Windowed)
{
    D3DDISPLAYMODE displayMode;
    IDirect3D9 *pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;

    HRESULT hr = pD3D->GetAdapterDisplayMode(m_AdapterIndex, &displayMode);
    if (FAILED(hr))
        return D3DFMT_UNKNOWN;

    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, displayMode.Format, D3DFMT_X8R8G8B8, Windowed)))
        return D3DFMT_X8R8G8B8;
    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, displayMode.Format, D3DFMT_X1R5G5B5, Windowed)))
        return D3DFMT_X1R5G5B5;
    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, displayMode.Format, D3DFMT_R5G6B5, Windowed)))
        return D3DFMT_R5G6B5;
    return D3DFMT_UNKNOWN;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestDepthFormat(D3DFORMAT pf, int ZBpp, int StencilBpp)
{
    VxImageDescEx desc;
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(pf);
    VxPixelFormat2ImageDesc(vxpf, desc);
    if (ZBpp <= 0)
        ZBpp = desc.BitsPerPixel;
    D3DFORMAT formats16[] = {
        D3DFMT_D15S1,
        D3DFMT_D16,
        D3DFMT_UNKNOWN
    };
    D3DFORMAT formats32[] = {
        D3DFMT_D24S8,
        D3DFMT_D24X4S4,
        D3DFMT_D32,
        D3DFMT_D24X8,
        D3DFMT_UNKNOWN
    };
    D3DFORMAT *formats = NULL;
    if (ZBpp <= 16)
        formats = formats16;
    else if (ZBpp <= 32)
        formats = formats32;
    for (D3DFORMAT *format = formats; *format != D3DFMT_UNKNOWN; ++format)
    {
        if (CheckDeviceFormat(pf, *format) && CheckDepthStencilMatch(pf, *format))
            return *format;
    }
    return (ZBpp == 16) ? formats16[0] : formats32[0];
}

CKBOOL CKDX9RasterizerDriver::CheckDeviceFormat(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat)
{
    return SUCCEEDED(static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9->CheckDeviceFormat(
        m_AdapterIndex, D3DDEVTYPE_HAL, AdapterFormat, 0, D3DRTYPE_TEXTURE, CheckFormat));
}

CKBOOL CKDX9RasterizerDriver::CheckDepthStencilMatch(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat)
{
    return SUCCEEDED(static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9->CheckDepthStencilMatch(
        m_AdapterIndex, D3DDEVTYPE_HAL, AdapterFormat, AdapterFormat, CheckFormat));
}
