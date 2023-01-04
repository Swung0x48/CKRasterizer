#include "CKGLRasterizer.h"

CKGLRasterizer::CKGLRasterizer(void): m_Init(FALSE)
{
	
}

CKGLRasterizer::~CKGLRasterizer(void)
{
}

XBOOL CKGLRasterizer::Start(WIN_HANDLE AppWnd)
{
    m_MainWindow = AppWnd;
    m_Init = TRUE;

    CKGLRasterizerDriver* driver = new CKGLRasterizerDriver(this);
    if (!driver->InitializeCaps())
        delete driver;
    if (driver)
        m_Drivers.PushBack(driver);
    return 1;
}

void CKGLRasterizer::Close(void)
{

}

CKRasterizer* CKGLRasterizerStart(WIN_HANDLE AppWnd) {
	CKRasterizer* rst = new CKGLRasterizer;
	if (!rst)
		return nullptr;
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
