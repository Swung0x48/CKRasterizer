#pragma once
#include "common_include.h"
#include "CKDX9RasterizerDriver.h"

typedef struct CKStageBlend {
	D3DTEXTUREOP Cop;
	CKDWORD		 Carg1;
	CKDWORD		 Carg2;
	D3DTEXTUREOP Aop;
	CKDWORD		 Aarg1;
	CKDWORD		 Aarg2;
} CKStageBlend;

class CKDX9Rasterizer : public CKRasterizer
{
public:
	CKDX9Rasterizer(HMODULE d3d9): m_Init(FALSE), m_D3D9(NULL), m_D3D9Handle(d3d9) {}
	virtual ~CKDX9Rasterizer(void);
	virtual XBOOL Start(WIN_HANDLE AppWnd);
	virtual void Close(void);
	void InitBlendStages();

public:
	XBOOL m_Init;
	HMODULE m_D3D9Handle;
	IDirect3D9* m_D3D9;

	CKStageBlend* m_BlendStages[256];
private:
	XArray<CKDX9RasterizerDriver*> m_Drivers;
};

CKRasterizer* CKDX9RasterizerStart(WIN_HANDLE AppWnd) {
	HMODULE handle = LoadLibraryA("d3d9.dll");
	if (handle) {
		CKRasterizer* rasterizer = new CKDX9Rasterizer(handle);
		if (!rasterizer)
			return NULL;
		if (!rasterizer->Start(AppWnd)) {
			delete rasterizer;
			FreeLibrary(handle);
			return nullptr;
		}
		return rasterizer;
	}
	return nullptr;
}

void CKDX9RasterizerClose(CKRasterizer* rst)
{
	if (rst)
	{
		rst->Close();
		delete rst;
	}
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo* info)
{
	info->StartFct = CKDX9RasterizerStart;
	info->CloseFct = CKDX9RasterizerClose;
	info->Desc = "DirectX 9 Rasterizer";
}