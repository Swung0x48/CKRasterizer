#include "CKDX12Rasterizer.h"
#include "CKDX12RasterizerCommon.h"

CKRasterizer *CKDX12RasterizerStart(WIN_HANDLE AppWnd)
{
    CKRasterizer *rasterizer = new CKDX12Rasterizer;
    if (!rasterizer->Start(AppWnd))
    {
        delete rasterizer;
        return nullptr;
    }
    return rasterizer;

}

void CKDX12RasterizerClose(CKRasterizer *rst)
{
    if (rst)
    {
        rst->Close();
        delete rst;
    }
}

bool D3DLogCall(HRESULT hr, const char *function, const char *file, int line)
{
    if (FAILED(hr))
    {
        LPTSTR error_text = NULL;
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&error_text, 0, NULL);
        std::string str = std::format("{}\n at {}\n{}:{:d}", error_text ? error_text : "", function, file, line);
        //        std::string str = std::string() + error_text + ": at " +
        //            function + " " + file + ":" + std::to_string(line);
        MessageBoxA(NULL, str.c_str(), "DirectX Error", NULL);
        LocalFree(error_text);
        assert(false);
        return false;
    }

    return true;
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo *info)
{
    info->StartFct = CKDX12RasterizerStart;
    info->CloseFct = CKDX12RasterizerClose;
    info->Desc = "DirectX 12 Rasterizer";
}

CKDX12Rasterizer::CKDX12Rasterizer(void) {}

CKDX12Rasterizer::~CKDX12Rasterizer(void) {}

XBOOL CKDX12Rasterizer::Start(WIN_HANDLE AppWnd)
{
    using Microsoft::WRL::ComPtr;
    m_MainWindow = AppWnd;

    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif
    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory));
    if (!D3DLogCall(hr, __FUNCTION__, __FILE__, __LINE__))
    {
        return FALSE;
    }

    BOOL allowTearing = FALSE;
    ComPtr<IDXGIFactory5> factory5;
    hr = m_Factory.As(&factory5);
    if (SUCCEEDED(hr))
    {
        // TODO: Put into context creation
        hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    }

    ComPtr<IDXGIFactory6> factory6;
    hr = m_Factory.As(&factory6);
    ComPtr<IDXGIAdapter1> adapter = nullptr;
    ComPtr<IDXGIOutput> output = nullptr;

    if (SUCCEEDED(hr))
    {
        m_DXGIVersionString = "1.6+";
        bool highperf = true;
        for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                 adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
             ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                bool headless = true;
                for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; ++j)
                {
                    headless = false;
                    auto *driver = new CKDX12RasterizerDriver(this);
                    if (!driver->InitializeCaps(adapter, output, highperf))
                        delete driver;
                    m_Drivers.PushBack(driver);
                }

                if (headless)
                {
                    auto *driver = new CKDX12RasterizerDriver(this);
                    if (!driver->InitializeCaps(adapter, nullptr, highperf))
                        delete driver;
                    m_Drivers.PushBack(driver);
                }
                highperf = false;
            }
        }
    }
    else
    {

        for (UINT i = 0; m_Factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; ++j)
            {
                CKDX12RasterizerDriver *driver = new CKDX12RasterizerDriver(this);
                if (!driver->InitializeCaps(adapter, output, false))
                    delete driver;
                m_Drivers.PushBack(driver);
            }
        }
    }

    return TRUE;
}

void CKDX12Rasterizer::Close(void)
{
}
