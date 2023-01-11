#include "CKGLRasterizer.h"

CKGLRasterizer::CKGLRasterizer(void): m_Init(FALSE)
{
	
}

CKGLRasterizer::~CKGLRasterizer(void)
{
}

// Callback for EnumDisplayMonitors in CKGLRasterizer::Start
//
static BOOL CALLBACK monitorCallback(HMONITOR handle,
                                     HDC dc,
                                     RECT* rect,
                                     LPARAM data)
{
    return GetMonitorInfo(handle, (LPMONITORINFO)data);
}

XBOOL CKGLRasterizer::Start(WIN_HANDLE AppWnd)
{
    m_MainWindow = AppWnd;
    m_Init = TRUE;
    DISPLAY_DEVICEA adapter;
    DISPLAY_DEVICEA display;
    MONITORINFOEXA mi;
    for (DWORD adapterIndex = 0; ; ++adapterIndex) {
        ZeroMemory(&adapter, sizeof(adapter));
        adapter.cb = sizeof(adapter);
        if (!EnumDisplayDevicesA(NULL, adapterIndex, &adapter, 0))
            return adapterIndex != 0;
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;
        for (DWORD displayIndex = 0; ; ++displayIndex) {
            ZeroMemory(&display, sizeof(display));
            display.cb = sizeof(display);
            if (!EnumDisplayDevicesA(adapter.DeviceName, displayIndex, &display, 0))
                break;
            if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;
            ZeroMemory(&mi, sizeof(mi));
            mi.cbSize = sizeof(mi);
            EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM)&mi);
            char* name = adapter.DeviceString;
            if (!name)
                return FALSE;
            CKGLRasterizerDriver* driver = new CKGLRasterizerDriver(this);
            driver->m_Desc = adapter.DeviceName;
            driver->m_Desc << " @ ";
            driver->m_Adapter = adapter;
            if (!driver->InitializeCaps())
                delete driver;
            if (driver)
                m_Drivers.PushBack(driver);
        }
    }
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
