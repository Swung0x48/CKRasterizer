#include "CKDX11Rasterizer.h"
#include "CKDX11RasterizerCommon.h"

CKRasterizer* CKDX11RasterizerStart(WIN_HANDLE AppWnd) {
	HMODULE handle = LoadLibraryA("d3d11.dll");
	if (handle) {
		CKRasterizer* rasterizer = new CKDX11Rasterizer;
        if (!rasterizer->Start(AppWnd)) {
			delete rasterizer;
			FreeLibrary(handle);
			return nullptr;
		}
		return rasterizer;
	}
	return nullptr;
}

void CKDX11RasterizerClose(CKRasterizer* rst)
{
	if (rst)
	{
		rst->Close();
		delete rst;
	}
}

bool D3DLogCall(HRESULT hr, const char* function, const char* file, int line)
{
    if (FAILED(hr))
    {
        LPTSTR error_text = NULL;
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       hr,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPTSTR)&error_text, 0,
                       NULL);
        std::string str = std::format("{}: at {} {}:{:d}",
                                      error_text,
                                      function,
                                      file,
                                      line);
//        std::string str = std::string() + error_text + ": at " +
//            function + " " + file + ":" + std::to_string(line);
        MessageBoxA(NULL, str.c_str(), "DirectX Error", NULL);
        LocalFree(error_text);
        return false;
    }

    return true;
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo* info)
{
	info->StartFct = CKDX11RasterizerStart;
	info->CloseFct = CKDX11RasterizerClose;
	info->Desc = "DirectX 11 Rasterizer";
}

CKDX11Rasterizer::CKDX11Rasterizer(void) {

}

CKDX11Rasterizer::~CKDX11Rasterizer(void)
{
}

XBOOL CKDX11Rasterizer::Start(WIN_HANDLE AppWnd)
{
	m_MainWindow = AppWnd;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&m_Factory);
	if (!D3DLogCall(hr, __FUNCTION__, __FILE__, __LINE__))
	{
	    return FALSE;
	}

	IDXGIAdapter1* adapter = nullptr;
	for (UINT i = 0;
		m_Factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
	    CKDX11RasterizerDriver* driver = new CKDX11RasterizerDriver(this);
		if (!driver->InitializeCaps(i, adapter))
			delete driver;
		m_Drivers.PushBack(driver);
	}

	return TRUE;
}

void CKDX11Rasterizer::Close(void)
{
	if (m_Factory)
	{
	    m_Factory->Release();
		m_Factory = nullptr;
	}
}
