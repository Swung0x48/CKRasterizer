#pragma once
#include <CKRasterizer.h>
#include <GL/glew.h>
#include <Windows.h>

class CKGLRasterizer : public CKRasterizer
{
public:
	CKGLRasterizer(void);
	virtual ~CKGLRasterizer(void);

	virtual XBOOL Start(WIN_HANDLE AppWnd);
	virtual void Close(void);

public:
	XBOOL m_Init;
};

CKRasterizer* CKGLRasterizerStart(WIN_HANDLE AppWnd) {
	CKRasterizer* rst = new CKGLRasterizer;
	if (!rst)
		return NULL;
	if (!rst->Start(AppWnd))
		delete rst;
	return rst;
}

void CKGLRasterizerClose(CKRasterizer* rst)
{
	if (rst)
	{
		rst->Close();
		delete rst;
	}
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo* info)
{
	info->StartFct = CKGLRasterizerStart;
	info->CloseFct = CKGLRasterizerClose;
	info->Desc = "OpenGL Rasterizer";
}