#include "CKGLRasterizer.h"
#include <CKContext.h>
#include <CKRenderManager.h>

CKGLRasterizerDriver::CKGLRasterizerDriver(CKGLRasterizer *rst)
{
	m_Owner = rst;
}

CKGLRasterizerDriver::~CKGLRasterizerDriver()
{
}

CKRasterizerContext *CKGLRasterizerDriver::CreateContext() {
    rst_ckctx->GetRenderManager()->SetRenderOptions("UseIndexBuffers", 1);
    CKGLRasterizerContext* context = new CKGLRasterizerContext;
    context->m_Driver = this;
    context->m_Owner = static_cast<CKGLRasterizer *>(m_Owner);
    context->m_DC = m_DC;
    context->m_RC = m_RC;
    context->m_HWND = m_HWND;
    m_Contexts.PushBack(context);
    return context;
}

bool operator==(const VxDisplayMode& a, const VxDisplayMode& b)
{
    return a.Bpp == b.Bpp &&
        a.Width == b.Width &&
        a.Height == b.Height &&
        a.RefreshRate == b.RefreshRate;
}

CKBOOL CKGLRasterizerDriver::InitializeCaps()
{
    DEVMODEA dm;
    
    // pretend we have a 640x480 @ 16bpp mode for compatibility reasons
    VxDisplayMode mode {640, 480, 16, 60};
    m_DisplayModes.PushBack(mode);
    
    ZeroMemory(&dm, sizeof(dm));
    for (DWORD modeIndex = 0; ; ++modeIndex)
    {
        if (!EnumDisplaySettingsA(m_Adapter.DeviceName, modeIndex, &dm))
            break;
        //ZeroMemory(&monitor, sizeof(monitor));
        //monitor.cbSize = sizeof(monitor);
        //EnumDisplayMonitors(NULL, &rect, monitorCallback, (LPARAM)&monitor);
        
        mode.Width = dm.dmPelsWidth;
        mode.Height = dm.dmPelsHeight;
        mode.Bpp = dm.dmBitsPerPel;
        mode.RefreshRate = dm.dmDisplayFrequency;
        if (m_DisplayModes.Back() != mode)
            m_DisplayModes.PushBack(mode);
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEXA wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = GL_WndProc;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = "Core";
    RegisterClassExA(&wcex);

    HWND fakeWND = CreateWindowA(
        "Core", "Fake Window",      // window class, title
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN, // style
        0, 0,                       // position x, y
        1, 1,                       // width, height
        NULL, NULL,                 // parent window, menu
        hInstance, NULL);           // instance, param
    
    HDC fakeDC = GetDC(fakeWND);        // Device Context
    PIXELFORMATDESCRIPTOR fakePFD;
    ZeroMemory(&fakePFD, sizeof(fakePFD));
    fakePFD.nSize = sizeof(fakePFD);
    fakePFD.nVersion = 1;
    fakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    fakePFD.iPixelType = PFD_TYPE_RGBA;
    fakePFD.cColorBits = 32;
    fakePFD.cAlphaBits = 8;
    fakePFD.cDepthBits = 24;
     
    int fakePFDID = ChoosePixelFormat(fakeDC, &fakePFD);
    if (fakePFDID == 0) {
        return 0;
    }

    if (SetPixelFormat(fakeDC, fakePFDID, &fakePFD) == false) {
        return 0;
    }
    HGLRC fakeRC = wglCreateContext(fakeDC);    // Rendering Context
    if (fakeRC == NULL) {
        return 0;
    }
     
    if (wglMakeCurrent(fakeDC, fakeRC) == false) {
        return 0;
    }

    m_DC = fakeDC;
    m_RC = fakeRC;
    m_HWND = fakeWND;
    m_Desc << glGetString(GL_RENDERER);
    m_Desc << " (OpenGL)";

    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_GLATTENUATIONMODEL;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    m_3DCaps.MaxActiveLights = 16;
    m_2DCaps.Family = CKRST_OPENGL;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
    m_DisplayModes.PushBack({640, 480, 16, 60});
    m_DisplayModes.PushBack({1024, 768, 32, 60});
    m_DisplayModes.PushBack({1600, 1200, 32, 60});
    m_DisplayModes.PushBack({1920, 1440, 32, 60});
    m_CapsUpToDate = TRUE;
    return 1;
}
