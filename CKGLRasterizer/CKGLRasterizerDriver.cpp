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

    ZeroMemory(&m_3DCaps, sizeof(m_3DCaps));
    ZeroMemory(&m_2DCaps, sizeof(m_2DCaps));
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_GLATTENUATIONMODEL;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    m_3DCaps.MaxNumberTextureStage = 8; //?
    m_3DCaps.MaxNumberBlendStage = 8;   //fake it until we make it
    m_3DCaps.MaxActiveLights = MAX_ACTIVE_LIGHTS;
    m_3DCaps.MinTextureWidth = 1;       //we are using texture of width 1 for blank textures
    m_3DCaps.MinTextureHeight = 1;      //so we know it must work... or do we?
    m_3DCaps.MaxTextureWidth = 1024;    //we know OpenGL guarantees this to be at least 1024...
    m_3DCaps.MaxTextureHeight = 1024;   //and I'm too lazy to create a context here...
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_DIRECTIONALLIGHTS;
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_TEXGEN;
    m_3DCaps.AlphaCmpCaps = 0xff;       //we have TECHNOLOGY
    m_3DCaps.ZCmpCaps = 0xff;           //who wouldn't be 0xff here?
    m_3DCaps.TextureAddressCaps = 0x1f; //everything
    m_3DCaps.TextureCaps |= CKRST_TEXTURECAPS_PERSPECTIVE; //not only do we support it, it's ALWAYS on
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_MASKZ;      //glDepthMask
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CONFORMANT; //because non-conformant OpenGL drivers don't exist /s
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLNONE;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCW;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCCW;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_ZTEST;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_FOGPIXEL; //vertex fog? what's that paleocene technology?
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WBUFFER;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WFOG;     //w fog hardcoded in shader
    m_3DCaps.SrcBlendCaps = 0x1fff;     //everything
    m_3DCaps.DestBlendCaps = 0x1fff;    //ditto
    m_2DCaps.Family = CKRST_OPENGL;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
    m_CapsUpToDate = TRUE;
    return 1;
}
