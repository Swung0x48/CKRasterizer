#include "CKGLRasterizer.h"
#define LOGGING 1
#define STEP 0
#define LOG_LOADTEXTURE 1
#define LOG_CREATETEXTURE 1
#define LOG_DRAWPRIMITIVE 0
#define LOG_DRAWPRIMITIVEVB 0
#define LOG_DRAWPRIMITIVEVBIB 0
#define LOG_SETTEXURESTAGESTATE 1
#define LOG_FLUSHCACHES 1
#define LOG_BATCHSTATS 1

#define USE_D3DSTATEBLOCKS 1

#if STEP
#include <conio.h>
static bool step_mode = false;
#endif

#if LOG_BATCHSTATS
static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;
#endif

const char* vertexShader = 
"#version 330 core\n"
"layout (location=0) in vec3 xyzw;\n"
"layout (location=1) in vec4 col;\n"
"layout (location=2) in vec2 texcoord;\n"
"out vec4 fragcol;\n"
"uniform mat4 modv;\n"
"uniform mat4 proj;\n"
"void main(){\n"
"    gl_Position=proj*modv*xyzw;\n"
"    fragcol=color;\n"
"}";

const char* fragShader =
"#version 330 core\n"
"in vec4 fragcol;\n"
"out vec4 color;\n"
"void main(){\n"
"    color=fragcol;\n"
"}";

CKGLRasterizerContext::CKGLRasterizerContext()
{
}

CKGLRasterizerContext::~CKGLRasterizerContext()
{
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;
    ReleaseDC((HWND)m_Window, m_DC);
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
#if (STEP) || (LOGGING)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
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
    //MessageBoxA(NULL, (char*)glGetString(GL_VERSION), (char*)glGetString(GL_VENDOR), NULL);
    m_Height = Height;
    m_Width = Width;
    m_Bpp = Bpp;
    m_Fullscreen = Fullscreen;
    m_RefreshRate = RefreshRate;
    m_ZBpp = Zbpp;
    m_StencilBpp = StencilBpp;

    if (m_Fullscreen)
        m_Driver->m_Owner->m_FullscreenContext = this;
    return 1;
}

CKBOOL CKGLRasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    return 1;
}

CKBOOL CKGLRasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT *rects)
{
    GLbitfield mask = 0;
    if (!m_TransparentMode && (Flags & CKRST_CTXCLEAR_COLOR) != 0 && m_Bpp)
            mask = GL_COLOR_BUFFER_BIT;
    if ((Flags & CKRST_CTXCLEAR_STENCIL) != 0 && m_StencilBpp)
        mask |= GL_STENCIL_BUFFER_BIT;
    if ((Flags & CKRST_CTXCLEAR_DEPTH) != 0 && m_ZBpp)
        mask |= GL_DEPTH_BUFFER_BIT;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::BackToFront(CKBOOL vsync)
{
#if LOGGING && LOG_BATCHSTATS
    fprintf(stderr, "batch stats: direct %d, vb %d, vbib %d\r", directbat, vbbat, vbibbat);
    directbat = 0;
    vbbat = 0;
    vbibbat = 0;
#endif
#if STEP
    int x = _getch();
    if (x == 'z')
        step_mode = true;
    else if (x == 'x')
        step_mode = false;
#endif
    glClearColor(0.5, 0.1, 0.3, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers(m_DC);

    return 1;
}

CKBOOL CKGLRasterizerContext::BeginScene()
{
    glBegin(GL_TRIANGLES);
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::EndScene()
{
    glEnd();
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    glEnable(GL_LIGHTING);
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    return CKRasterizerContext::SetMaterial(mat);
}

CKBOOL CKGLRasterizerContext::SetViewport(CKViewportData *data)
{
    glViewport(data->ViewX, data->ViewY, data->ViewWidth, data->ViewHeight);
    assert(glGetError() == GL_NO_ERROR);
    glDepthRange(data->ViewZMin, data->ViewZMax);
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    CKDWORD UnityMatrixMask = 0;
    switch (Type)
    {
        case VXMATRIX_WORLD:
            m_WorldMatrix = Mat;
            UnityMatrixMask = WORLD_TRANSFORM;
		    Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            break;
        case VXMATRIX_VIEW:
            m_ViewMatrix = Mat;
            UnityMatrixMask = VIEW_TRANSFORM;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate = 0;
            break;
        case VXMATRIX_PROJECTION:
            m_ProjectionMatrix = Mat;
            UnityMatrixMask = PROJ_TRANSFORM;
            m_MatrixUptodate = 0;
            break;
        case VXMATRIX_TEXTURE0:
        case VXMATRIX_TEXTURE1:
        case VXMATRIX_TEXTURE2:
        case VXMATRIX_TEXTURE3:
        case VXMATRIX_TEXTURE4:
        case VXMATRIX_TEXTURE5:
        case VXMATRIX_TEXTURE6:
        case VXMATRIX_TEXTURE7:
            UnityMatrixMask = TEXTURE0_TRANSFORM << (Type - TEXTURE1_TRANSFORM);
            break;
        default:
            return FALSE;
    }
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= UnityMatrixMask;
    } else
    {
        m_UnityMatrixMask &= ~UnityMatrixMask;
    }
    return 1;
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
#if LOGGING && LOG_SETTEXTURE
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
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
#if LOGGING && LOG_DRAWPRIMITIVE
    fprintf(stderr, "drawprimitive ib %d\n", indexcount);
#endif
#if LOG_BATCHSTATS
    ++directbat;
#endif
    return 1;
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
    CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if LOGGING && LOG_DRAWPRIMITIVEVB
    fprintf(stderr, "drawprimitive vb %d %d\n", VertexCount, indexcount);
#endif
#if STEP
    if (step_mode)
        _getch();
#endif
#if LOG_BATCHSTATS
    ++vbbat;
#endif
    return 1;
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
    CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if LOGGING && LOG_DRAWPRIMITIVEVBIB
    fprintf(stderr, "drawprimitive vbib %d %d\n", VertexCount, Indexcount);
#endif
#if STEP
    if (step_mode)
        _getch();
#endif
#if LOG_BATCHSTATS
    ++vbibbat;
#endif
    return CKRasterizerContext::DrawPrimitiveVBIB(pType, VB, IB, MinVIndex, VertexCount, StartIndex, Indexcount);
}

CKBOOL CKGLRasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    int result;

    if (ObjIndex >= m_Textures.Size())
        return 0;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
        {
            return 0;
            result = CreateSprite(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
            CKSpriteDesc* desc = m_Sprites[ObjIndex];
            fprintf(stderr, "idx: %d\n", ObjIndex);
            for (auto it = desc->Textures.Begin(); it != desc->Textures.End(); ++it)
            {
                fprintf(stderr, "(%d,%d) WxH: %dx%d, SWxSH: %dx%d\n", it->x, it->y, it->w, it->h, it->sw, it->sh);
            }
            fprintf(stderr, "---\n");
            break;
        }
        case CKRST_OBJ_VERTEXBUFFER:
            result = CreateVertexBuffer(ObjIndex, static_cast<CKVertexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_INDEXBUFFER:
            result = CreateIndexBuffer(ObjIndex, static_cast<CKIndexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXSHADER:
            result =
                CreateVertexShader(ObjIndex, static_cast<CKVertexShaderDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_PIXELSHADER:
            result =
                CreatePixelShader(ObjIndex, static_cast<CKPixelShaderDesc *>(DesiredFormat));
            break;
        default:
            return 0;
    }
    return result;
}

void * CKGLRasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
    CKRST_LOCKFLAGS Lock)
{
    return NULL;
}

CKBOOL CKGLRasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    return 0;
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
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return 0;
    CKGLVertexBufferDesc* desc = static_cast<CKGLVertexBufferDesc *>(m_VertexBuffers[VB]);
    delete desc;
    desc = new CKGLVertexBufferDesc;
    desc->m_VertexSize = DesiredFormat->m_VertexSize;
    desc->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    desc->m_Flags = DesiredFormat->m_CurrentVCount;
    desc->m_MaxVertexCount = DesiredFormat->m_CurrentVCount;
    desc->m_VertexFormat = DesiredFormat->m_VertexFormat;

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBufferData(GL_ARRAY_BUFFER, 
        DesiredFormat->m_MaxVertexCount * DesiredFormat->m_VertexSize, 
        nullptr, GL_STATIC_DRAW);
    desc->GLBuffer = vbo;
    m_VertexBuffers[VB] = desc;
    return 1;
}

CKBOOL CKGLRasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return 0;
    CKGLIndexBufferDesc* desc = static_cast<CKGLIndexBufferDesc *>(m_IndexBuffers[IB]);
    delete desc;
    desc = new CKGLIndexBufferDesc;
    desc->m_Flags = DesiredFormat->m_Flags;
    desc->m_CurrentICount = DesiredFormat->m_CurrentICount;
    desc->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount;

    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * DesiredFormat->m_MaxIndexCount, nullptr, GL_STATIC_DRAW);
    desc->GLBuffer = ibo;
    m_IndexBuffers[IB] = desc;
    return 1;
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

void CKGLRasterizerContext::ReleaseBuffers()
{
    
}

void CKGLRasterizerContext::ClearStreamCache()
{
}

void CKGLRasterizerContext::ReleaseScreenBackup()
{
}
