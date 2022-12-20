#include "CKDX9Rasterizer.h"

static const D3DFORMAT AdapterFormats[] = {
	D3DFMT_A8R8G8B8,
	D3DFMT_X8R8G8B8,
	D3DFMT_R5G6B5,
	D3DFMT_X1R5G5B5,
	D3DFMT_A1R5G5B5
};

CKDX9RasterizerDriver::CKDX9RasterizerDriver(CKDX9Rasterizer* rst)
{
	m_Owner = rst;
}

CKDX9RasterizerDriver::~CKDX9RasterizerDriver()
{
}

CKRasterizerContext* CKDX9RasterizerDriver::CreateContext()
{
	CKDX9RasterizerContext* context = new CKDX9RasterizerContext();
    context->m_Driver = this;
    context->m_Owner = static_cast<CKDX9Rasterizer *>(m_Owner);
	m_Contexts.PushBack(context);
	return context;
}


struct DisplayModeWithTexture {
	UINT Width;
	UINT Height;
	CKDX9TextureDesc Desc;
};

UINT CKDX9RasterizerDriver::BytesPerPixel(D3DFORMAT Format)
{
	switch (DWORD(Format))
	{
	case D3DFMT_DXT1:
		// Size is negative to indicate DXT; and indicates
		// the size of the block
		return (CKDWORD)(-8);
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		// Size is negative to indicate DXT; and indicates
		// the size of the block
		return (CKDWORD)(-16);


	case D3DFMT_A32B32G32R32F:
		return 16;

	case D3DFMT_A16B16G16R16:
	case D3DFMT_Q16W16V16U16:
	case D3DFMT_A16B16G16R16F:
	case D3DFMT_G32R32F:
	case D3DFMT_MULTI2_ARGB8:
		return 8;

	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_D24X4S4:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
	case D3DFMT_A2W10V10U10:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_X8B8G8R8:
	case D3DFMT_G16R16:
	case D3DFMT_D24X8:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_G16R16F:
	case D3DFMT_R32F:
	case D3DFMT_D32F_LOCKABLE:
	case D3DFMT_D24FS8:
	case D3DFMT_D32_LOCKABLE:
		return 4;

	case D3DFMT_R8G8B8:
		return 3;

	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_A8L8:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_D16:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_A8P8:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
	case D3DFMT_X4R4G4B4:
	case D3DFMT_CxV8U8:
	case D3DFMT_L16:
	case D3DFMT_R16F:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_G8R8_G8B8:
		return 2;

	case D3DFMT_P8:
	case D3DFMT_L8:
	case D3DFMT_R3G3B2:
	case D3DFMT_A4L4:
	case D3DFMT_A8:
	case D3DFMT_A1:
	case D3DFMT_S8_LOCKABLE:
		return 1;

	default:
		return 0;
	};
}; // BytesPerPixel

BOOL CKDX9RasterizerDriver::InitializeCaps(int AdapterIndex, D3DDEVTYPE DevType)
{
	m_AdapterIndex = AdapterIndex;
	m_Inited = TRUE;
    IDirect3D9Ex* pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;
	pD3D->GetAdapterIdentifier(AdapterIndex, D3DENUM_WHQL_LEVEL, &m_D3DIdentifier);
	D3DDISPLAYMODE DisplayMode;
	pD3D->GetAdapterDisplayMode(AdapterIndex, &DisplayMode);

	for (D3DFORMAT Format : AdapterFormats) {
		UINT AdapterModeCount = pD3D->GetAdapterModeCount(AdapterIndex, Format);
		if (AdapterModeCount > 0) {
			m_RenderFormats.PushBack(Format);
			CKTextureDesc desc;
			D3DFormatToTextureDesc(Format, &desc);
			m_TextureFormats.PushBack(desc);
		}
		for (UINT i = 0; i < AdapterModeCount; ++i)
		{
			pD3D->EnumAdapterModes(AdapterIndex, Format, i, &DisplayMode);
			int width = DisplayMode.Width;
			int height = DisplayMode.Height;
			if (DisplayMode.Width >= 640 && DisplayMode.Height >= 400) {
				// TODO: ???
				// persumably: populate m_RenderFormats, m_DisplayModes, m_TextureFormats
				m_DisplayModes.PushBack({
					width, height,
					(int) BytesPerPixel(DisplayMode.Format) * 8,
					(int)DisplayMode.RefreshRate });
			}
		}
	}
	assert(SUCCEEDED(pD3D->GetDeviceCaps(AdapterIndex, D3DDEVTYPE_HAL, &m_D3DCaps)));

	// TODO: Populate 2D/3D capabilities

	m_Hardware = 1;
	m_3DCaps.StencilCaps = m_D3DCaps.StencilCaps;
	m_3DCaps.DevCaps = m_D3DCaps.DevCaps;
	m_3DCaps.MinTextureWidth = 1;
	m_3DCaps.MinTextureHeight = 1;
	m_3DCaps.MaxTextureWidth = m_D3DCaps.MaxTextureWidth;
	m_3DCaps.MaxTextureHeight = m_D3DCaps.MaxTextureHeight;
	m_3DCaps.MaxTextureRatio = m_D3DCaps.MaxTextureAspectRatio;
	if (!m_D3DCaps.MaxTextureAspectRatio) {
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
	m_3DCaps.CKRasterizerSpecificCaps = (
		CKRST_SPECIFICCAPS_SPRITEASTEXTURES |
		CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER |
		CKRST_SPECIFICCAPS_GLATTENUATIONMODEL |
		CKRST_SPECIFICCAPS_COPYTEXTURE |
		CKRST_SPECIFICCAPS_DX8 |
		//CKRST_SPECIFICCAPS_DX9 |
		CKRST_SPECIFICCAPS_CANDOINDEXBUFFER);
	m_2DCaps.AvailableVideoMemory = 0;
	m_2DCaps.MaxVideoMemory = 0;
	m_2DCaps.Family = CKRST_DIRECTX;
	m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);

	DWORD Caps2 = m_D3DCaps.Caps2;
	if ((Caps2 & 0x80000) != 0 && !AdapterIndex) // TODO: Unknown enum
		m_2DCaps.Caps = (CKRST_2DCAPS_WINDOWED | CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
    HMONITOR hMonitor = pD3D->GetAdapterMonitor(AdapterIndex);
    MONITORINFOEXA Info;
    Info.cbSize = sizeof(MONITORINFOEXA);
    if (GetMonitorInfoA(hMonitor, &Info))
    {
        m_Desc = &Info.szDevice[4];
        m_Desc << " (" << (int)(Info.rcMonitor.right - Info.rcMonitor.left) << "x"
               << (int)(Info.rcMonitor.bottom - Info.rcMonitor.top) << ")";
        m_Desc << " @ " << m_D3DIdentifier.Description;
    } else
    {
        m_Desc = m_D3DIdentifier.Description;
    }
	XWORD pos = m_Desc.Find('\\');
	if (pos != XString::NOTFOUND)
		m_Desc = m_Desc.Crop(0, pos);

	if ((m_D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0) {
		m_IsHTL = TRUE;
		m_Desc << " (T&L DX9)";
		m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
	} else {
		m_IsHTL = FALSE;
		m_Desc << " (Hardware DX9)";
		m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARE;
	}
	m_CapsUpToDate = TRUE;
	return TRUE;
}

BOOL CKDX9RasterizerDriver::IsTextureFormatOk(D3DFORMAT TextureFormat, D3DFORMAT AdapterFormat, DWORD Usage)
{
	return 0;
}


D3DFORMAT CKDX9RasterizerDriver::FindNearestTextureFormat(CKTextureDesc* desc)
{
	// TODO: AI generated
    DWORD flags = desc->Format.Flags;
    int min_delta = 64;
    int bpp = desc->Format.BitsPerPixel;
    unsigned int BitCount = GetBitCount(desc->Format.AlphaMask);

    if (flags == 19)
    {
        BitCount = 1;
        bpp = 16;
    }
    else if (flags >= 0x13 && flags <= 0x17)
    {
        BitCount = 8;
        bpp = 32;
    }

    CKTextureDesc *desc = NULL;
    for (CKTextureDesc *iter = this->m_TextureFormats.Begin(); iter < m_TextureFormats.End(); ++iter)
    {
        if (bpp == iter->Format.BitsPerPixel && GetBitCount(iter->Format.AlphaMask) == BitCount)
        {
            return TextureDescToD3DFormat(iter);
        }

        if (!desc->Format.AlphaMask || iter->Format.AlphaMask)
        {
            int cur_delta = abs(bpp - iter->Format.BitsPerPixel);
            if (cur_delta < min_delta)
            {
                min_delta = cur_delta;
                desc = iter;
            }
        }
    }
    if (desc)
    {
        return TextureDescToD3DFormat(desc);
    }
    return D3DFMT_UNKNOWN;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestRenderTargetFormat(int Bpp, BOOL Windowed)
{
    D3DDISPLAYMODEEX DisplayMode;
    DisplayMode.Size = sizeof(D3DDISPLAYMODEEX);
    IDirect3D9Ex* pD3D = static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9;

    HRESULT result = pD3D->GetAdapterDisplayModeEx(m_AdapterIndex, &DisplayMode, NULL);
    if (FAILED(result))
        return D3DFMT_UNKNOWN;

    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, DisplayMode.Format, D3DFMT_X8R8G8B8, Windowed)))
        return D3DFMT_X8R8G8B8;
    if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, DisplayMode.Format, D3DFMT_X1R5G5B5, Windowed)))
        return D3DFMT_X1R5G5B5;
	if (SUCCEEDED(pD3D->CheckDeviceType(m_AdapterIndex, D3DDEVTYPE_HAL, DisplayMode.Format, D3DFMT_R5G6B5, Windowed)))
        return D3DFMT_R5G6B5;
	return D3DFMT_UNKNOWN;
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestDepthFormat(D3DFORMAT pf, int ZBpp, int StencilBpp)
{
	VxImageDescEx Desc;
	VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(pf);
	VxPixelFormat2ImageDesc(vxpf, Desc);
	if (ZBpp <= 0)
		ZBpp = Desc.BitsPerPixel;
    D3DFORMAT Formats16[] = {
        D3DFMT_D15S1,
        D3DFMT_D16,
        D3DFMT_UNKNOWN
    };
    D3DFORMAT Formats32[] = {
        D3DFMT_D24S8,
        D3DFMT_D24X4S4,
        D3DFMT_D32,
        D3DFMT_D24X8,
		D3DFMT_UNKNOWN
    };
    D3DFORMAT *Formats = NULL;
    if (ZBpp <= 16)
        Formats = Formats16;
    else if (ZBpp <= 32)
		Formats = Formats32;
    for (D3DFORMAT *Format = Formats; *Format != D3DFMT_UNKNOWN; ++Format)
    {
        if (CheckDeviceFormat(pf, *Format) &&
			CheckDepthStencilMatch(pf, *Format))
            return *Format;
    }
    return (ZBpp == 16) ? Formats16[0] : Formats32[0];
}


BOOL CKDX9RasterizerDriver::CheckDeviceFormat(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat) {
	return SUCCEEDED(static_cast<CKDX9Rasterizer*>(m_Owner)->m_D3D9->CheckDeviceFormat(
		m_AdapterIndex,
		D3DDEVTYPE_HAL,
		AdapterFormat,
		0,
		D3DRTYPE_TEXTURE,
		CheckFormat
	));
}

BOOL CKDX9RasterizerDriver::CheckDepthStencilMatch(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat)
{
    return SUCCEEDED(static_cast<CKDX9Rasterizer *>(m_Owner)->m_D3D9->CheckDepthStencilMatch(
		this->m_AdapterIndex,
		D3DDEVTYPE_HAL,
		AdapterFormat,
		AdapterFormat,
		CheckFormat));
}
