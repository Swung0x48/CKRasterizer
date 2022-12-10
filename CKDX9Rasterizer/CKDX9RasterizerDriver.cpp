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

			}
		}
	}
	return 0;
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
