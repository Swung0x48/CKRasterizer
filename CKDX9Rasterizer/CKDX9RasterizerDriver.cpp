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
	return nullptr;
}

BOOL CKDX9RasterizerDriver::InitializeCaps(int AdapterIndex, D3DDEVTYPE DevType)
{
	m_AdapterIndex = AdapterIndex;
	m_Inited = TRUE;
	LPDIRECT3D9 pD3D = static_cast<CKDX9Rasterizer*>(m_Owner)->m_D3D9;
	pD3D->GetAdapterIdentifier(AdapterIndex, D3DENUM_WHQL_LEVEL, &m_D3DIdentifier);
	D3DDISPLAYMODE DisplayMode;
	pD3D->GetAdapterDisplayMode(AdapterIndex, &DisplayMode);
	for (D3DFORMAT Format : AdapterFormats) {
		UINT AdapterModeCount = pD3D->GetAdapterModeCount(AdapterIndex, Format);
		for (UINT Mode = 0; Mode < AdapterModeCount; ++Mode)
		{
			pD3D->EnumAdapterModes(AdapterIndex, Format, Mode, &DisplayMode);
			if (DisplayMode.Width >= 640 && DisplayMode.Height >= 400) {
				// TODO: ???
				// persumably: populate m_RenderFormats, m_DisplayModes, m_TextureFormats
			}
		}
	}
	pD3D->GetDeviceCaps(AdapterIndex, D3DDEVTYPE_HAL, &m_D3DCaps);

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
		CKRST_SPECIFICCAPS_DX9 |
		CKRST_SPECIFICCAPS_CANDOINDEXBUFFER);
	m_2DCaps.AvailableVideoMemory = 0;
	m_2DCaps.MaxVideoMemory = 0;
	m_2DCaps.Family = CKRST_DIRECTX;
	m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);

	DWORD Caps2 = m_D3DCaps.Caps2;
	if ((Caps2 & 0x80000) != 0 && !AdapterIndex) // TODO: Unknown enum
		this->m_2DCaps.Caps = (CKRST_2DCAPS_WINDOWED | CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
	m_Desc = m_D3DIdentifier.Description;
	XWORD pos = m_Desc.Find('\\');
	if (pos != XString::NOTFOUND)
		m_Desc = m_Desc.Crop(0, pos);

	if ((m_D3DCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0) {
		m_IsHTL = TRUE;
		m_Desc << "(T&L DX9)";
		m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
	} else {
		m_IsHTL = FALSE;
		m_Desc << "(Hardware DX9)";
		m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARE;
	}
	m_CapsUpToDate = TRUE;
	return 1;
}

BOOL CKDX9RasterizerDriver::IsTextureFormatOk(D3DFORMAT TextureFormat, D3DFORMAT AdapterFormat, DWORD Usage)
{
	return 0;
}


D3DFORMAT CKDX9RasterizerDriver::FindNearestTextureFormat(CKTextureDesc* desc)
{
	return D3DFORMAT();
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestRenderTargetFormat(int Bpp, BOOL Windowed)
{
	return D3DFORMAT();
}

D3DFORMAT CKDX9RasterizerDriver::FindNearestDepthFormat(D3DFORMAT pf, int ZBpp, int StencilBpp)
{
	return D3DFORMAT();
}


BOOL CKDX9RasterizerDriver::CheckDeviceFormat(D3DFORMAT AdapterFormat, D3DFORMAT CheckFormat) {
	return static_cast<CKDX9Rasterizer*>(m_Owner)->m_D3D9->CheckDeviceFormat(
		m_AdapterIndex,
		D3DDEVTYPE_HAL,
		AdapterFormat,
		0,
		D3DRTYPE_TEXTURE,
		CheckFormat
	) >= 0;
}
