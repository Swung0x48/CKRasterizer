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
        std::string str = std::format("{}\n at {}\n{}:{:d}", error_text ? error_text : "",
                                      function,
                                      file,
                                      line);
//        std::string str = std::string() + error_text + ": at " +
//            function + " " + file + ":" + std::to_string(line);
        MessageBoxA(NULL, str.c_str(), "DirectX Error", NULL);
        LocalFree(error_text);
        assert(false);
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
	using Microsoft::WRL::ComPtr;
	m_MainWindow = AppWnd;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_Factory));
	if (!D3DLogCall(hr, __FUNCTION__, __FILE__, __LINE__))
	{
	    return FALSE;
	}

    BOOL allowTearing = FALSE;
    ComPtr<IDXGIFactory2> factory2;
    hr = m_Factory.As(&factory2);
    if (SUCCEEDED(hr))
    {
        m_FlipPresent = TRUE;
        m_DXGIVersionString = "1.2";

        ComPtr<IDXGIFactory5> factory5;
        hr = m_Factory.As(&factory5);
        if (SUCCEEDED(hr))
        {
            m_DXGIVersionString = "1.5+";
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        }
    }
    
    m_TearingSupport = SUCCEEDED(hr) && allowTearing;

	ComPtr<IDXGIAdapter1> adapter = nullptr;
    ComPtr<IDXGIOutput> output = nullptr;
	for (UINT i = 0;
		m_Factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
        for (UINT j = 0;
			adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND;
			++j)
        {
            CKDX11RasterizerDriver *driver = new CKDX11RasterizerDriver(this);
            if (!driver->InitializeCaps(adapter, output))
                delete driver;
            m_Drivers.PushBack(driver);
        }
	}

	return TRUE;
}

void CKDX11Rasterizer::Close(void)
{
    while (!m_Drivers.IsEmpty())
    {
        auto *driver = m_Drivers.PopBack();
        delete driver;
	}
}
