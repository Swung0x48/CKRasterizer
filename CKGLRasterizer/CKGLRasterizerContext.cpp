#include "CKGLRasterizer.h"


CKGLRasterizerContext::CKGLRasterizerContext()
{
}

CKGLRasterizerContext::~CKGLRasterizerContext()
{
    //glfwDestroyWindow(m_GLFWwindow);
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;
}

LRESULT WINAPI GL_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ::PostQuitMessage(0);
            }
            break;
        case WM_CLOSE:
            ::PostQuitMessage(0);
            break;
        default:
            return ::DefWindowProc(hWnd, Msg, wParam, lParam);
    }
    return 0;
}

CKBOOL CKGLRasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
    CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
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
    HGLRC fakeRC = wglCreateContext(fakeDC);    // Rendering Contex
 
    if (fakeRC == NULL) {
        return 0;
    }
     
    if (wglMakeCurrent(fakeDC, fakeRC) == false) {
        return 0;
    }

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
    if (wglChoosePixelFormatARB == nullptr) {
        return 0;
    }
     
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (wglCreateContextAttribsARB == nullptr) {
        return 0;
    }
    
    const int pixelAttribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        WGL_SAMPLES_ARB, 4,
        0
    };
    HDC DC = GetDC((HWND)Window);
    
    int pixelFormatID; UINT numFormats;
    bool status = wglChoosePixelFormatARB(DC, pixelAttribs, NULL, 1, &pixelFormatID, &numFormats);
     
    if (status == false || numFormats == 0) {
        return 0;
    }
    PIXELFORMATDESCRIPTOR PFD;
    DescribePixelFormat(DC, pixelFormatID, sizeof(PFD), &PFD);
    SetPixelFormat(DC, pixelFormatID, &PFD);
    const int major_min = 4, minor_min = 5;
    int  contextAttribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, major_min,
        WGL_CONTEXT_MINOR_VERSION_ARB, minor_min,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        0
    };
    m_DC = DC;
     
    HGLRC RC = wglCreateContextAttribsARB(DC, NULL, contextAttribs);
    if (RC == NULL) {
        return 0;
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(fakeRC);
    ReleaseDC(fakeWND, fakeDC);
    DestroyWindow(fakeWND);
    if (!wglMakeCurrent(DC, RC)) {
        return 0;
    }
    if (glewInit() != GLEW_OK)
    {
        return 0;
    }
    SetWindowTextA((HWND)Window, (char*)glGetString(GL_VERSION));
    ShowWindow((HWND)Window, SW_SHOW);
    MessageBoxA(NULL, (char*)glGetString(GL_VERSION), (char*)glGetString(GL_VENDOR), NULL);
    if (m_Fullscreen)
        m_Driver->m_Owner->m_FullscreenContext = this;
    /*if (!glfwInit())
        return 0;
    m_GLFWwindow = glfwCreateWindow(Width, Height, "Virtools OpenGL Context", NULL, NULL);
    if (!m_GLFWwindow)
    {
        glfwTerminate();
        return 0;
    }
    m_Window = glfwGetWin32Window(m_GLFWwindow);
    LONG PrevStyle = GetWindowLongA((HWND)m_Window, GWL_STYLE);
    SetWindowLongA((HWND)m_Window, -16, PrevStyle | WS_CHILDWINDOW);
    glfwMakeContextCurrent(m_GLFWwindow);
    if (glewInit() != GLEW_OK)
    {
        return 0;
    }
    CKRECT Rect;
    VxGetWindowRect(Window, &Rect);
	WIN_HANDLE Parent = VxGetParent(Window);
	VxScreenToClient(Parent, reinterpret_cast<CKPOINT*>(&Rect));
	VxScreenToClient(Parent, reinterpret_cast<CKPOINT*>(&Rect.right));*/
    return 1;
}

CKBOOL CKGLRasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    return 1;
}

CKBOOL CKGLRasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT *rects)
{
    glClearColor(0.129f, 0.586f, 0.949f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return 1;
}

CKBOOL CKGLRasterizerContext::BackToFront(CKBOOL vsync)
{
    glBegin(GL_TRIANGLES);

    glColor3f(1.0, 0.0, 0.0);
    glVertex3f(0.0, 1.0, 0.0);

    glColor3f(0.0, 1.0, 0.0);
    glVertex3f(-1.0, -1.0, 0.0);

    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(1.0, -1.0, 0.0);

    glEnd();

    SwapBuffers(m_DC);
    /*glfwSwapBuffers(m_GLFWwindow);
    glfwPollEvents();*/
    return 1;
}

CKBOOL CKGLRasterizerContext::BeginScene()
{
    return CKRasterizerContext::BeginScene();
}

CKBOOL CKGLRasterizerContext::EndScene()
{
    return CKRasterizerContext::EndScene();
}

CKBOOL CKGLRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    return CKRasterizerContext::SetLight(Light, data);
}

CKBOOL CKGLRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    return CKRasterizerContext::EnableLight(Light, Enable);
}

CKBOOL CKGLRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    return CKRasterizerContext::SetMaterial(mat);
}

CKBOOL CKGLRasterizerContext::SetViewport(CKViewportData *data)
{
    return CKRasterizerContext::SetViewport(data);
}

CKBOOL CKGLRasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    return CKRasterizerContext::SetTransformMatrix(Type, Mat);
}

CKBOOL CKGLRasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    return CKRasterizerContext::SetRenderState(State, Value);
}

CKBOOL CKGLRasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}

CKBOOL CKGLRasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
    return CKRasterizerContext::SetTexture(Texture, Stage);
}

CKBOOL CKGLRasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    return CKRasterizerContext::SetTextureStageState(Stage, Tss, Value);
}

CKBOOL CKGLRasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    return CKRasterizerContext::SetVertexShader(VShaderIndex);
}

CKBOOL CKGLRasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    return CKRasterizerContext::SetPixelShader(PShaderIndex);
}

CKBOOL CKGLRasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}

CKBOOL CKGLRasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}

CKBOOL CKGLRasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount,
    VxDrawPrimitiveData *data)
{
    return CKRasterizerContext::DrawPrimitive(pType, indices, indexcount, data);
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
    CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
    return CKRasterizerContext::DrawPrimitiveVB(pType, VertexBuffer, StartIndex, VertexCount, indices, indexcount);
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
    CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
    return CKRasterizerContext::DrawPrimitiveVBIB(pType, VB, IB, MinVIndex, VertexCount, StartIndex, Indexcount);
}

CKBOOL CKGLRasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    return CKRasterizerContext::CreateObject(ObjIndex, Type, DesiredFormat);
}

void * CKGLRasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
    CKRST_LOCKFLAGS Lock)
{
    return CKRasterizerContext::LockVertexBuffer(VB, StartVertex, VertexCount, Lock);
}

CKBOOL CKGLRasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    return CKRasterizerContext::UnlockVertexBuffer(VB);
}

CKBOOL CKGLRasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    return CKRasterizerContext::LoadTexture(Texture, SurfDesc, miplevel);
}

CKBOOL CKGLRasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}

CKBOOL CKGLRasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face,
    CKBOOL GenerateMipMap)
{
    return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face, GenerateMipMap);
}

CKBOOL CKGLRasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    return CKRasterizerContext::DrawSprite(Sprite, src, dst);
}

int CKGLRasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyToMemoryBuffer(rect, buffer, img_desc);
}

int CKGLRasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyFromMemoryBuffer(rect, buffer, img_desc);
}

CKBOOL CKGLRasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)
{
    return CKRasterizerContext::SetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

CKBOOL CKGLRasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)
{
    return CKRasterizerContext::GetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

void * CKGLRasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    return CKRasterizerContext::LockIndexBuffer(IB, StartIndex, IndexCount, Lock);
}

CKBOOL CKGLRasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    return CKRasterizerContext::UnlockIndexBuffer(IB);
}

CKBOOL CKGLRasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
    return 1;
}

CKBOOL CKGLRasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
    return 1;
}

CKBOOL CKGLRasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{    return 1;

}

CKBOOL CKGLRasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    return 1;
}

CKBOOL CKGLRasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{    return 1;

}

void CKGLRasterizerContext::FlushCaches()
{
}

void CKGLRasterizerContext::FlushNonManagedObjects()
{
}

void CKGLRasterizerContext::ReleaseStateBlocks()
{
}

void CKGLRasterizerContext::ReleaseIndexBuffers()
{
}

void CKGLRasterizerContext::ClearStreamCache()
{
}

void CKGLRasterizerContext::ReleaseScreenBackup()
{
}
