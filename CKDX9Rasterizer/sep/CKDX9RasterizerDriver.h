#pragma once
#include "common_include.h"

class CKDX9RasterizerDriver : public CKRasterizerDriver
{
public:
	CKDX9RasterizerDriver(CKDX9Rasterizer* rst);
	virtual ~CKDX9RasterizerDriver();

	//--- Contexts
	virtual CKRasterizerContext* CreateContext();

	BOOL InitializeCaps(CKDX9Rasterizer* rasterizer);
	BOOL IsTextureFormatOk(D3DFORMAT TextureFormat, D3DFORMAT AdapterFormat);

	D3DFORMAT FindNearestTextureFormat(CKTextureDesc* desc);
	D3DFORMAT FindNearestRenderTargetFormat(int Bpp, BOOL Windowed);
	D3DFORMAT FindNearestDepthFormat(D3DFORMAT pf, int ZBpp, int StencilBpp);


public:
	BOOL					 m_Inited;
	UINT					 m_AdapterIndex;
	D3DCAPS9				 m_D3DCaps;
	D3DADAPTER_IDENTIFIER9	 m_D3DIdentifier;
	XArray<D3DFORMAT>		 m_RenderFormats;
	BOOL					 m_IsHTL;	// Transfom & Lighting
private:

};