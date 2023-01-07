#include "CKGLRasterizer.h"
#define LOGGING 1
#define STEP 0
#define LOG_LOADTEXTURE 0
#define LOG_CREATETEXTURE 0
#define LOG_CREATEBUFFER 0
#define LOG_DRAWPRIMITIVE 0
#define LOG_DRAWPRIMITIVEVB 0
#define LOG_DRAWPRIMITIVEVBIB 0
#define LOG_SETTEXURESTAGESTATE 0
#define LOG_FLUSHCACHES 0
#define LOG_BATCHSTATS 1

#if STEP
#include <conio.h>
static bool step_mode = false;
#endif

#if LOG_BATCHSTATS
static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;
#endif

void GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool GLLogCall(const char* function, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        std::string str = std::to_string(error) + ": at " + 
            function + " " + file + ":" + std::to_string(line);
        MessageBoxA(NULL, str.c_str(), "OpenGL Error", NULL);
        return false;
    }

    
    return true;
}

const char* vertexShader = 
R"(#version 330 core
layout (location=0) in vec4 xyzw;
layout (location=1) in vec3 normal; //!!TODO
layout (location=2) in vec4 color;
layout (location=3) in vec4 spec_color; //!!TODO
layout (location=4) in vec2 texcoord;
out vec4 fragcol;
out vec2 ftexcoord;
uniform bool is_transformed;
uniform mat4 world;
uniform mat4 view;
uniform mat4 proj;
void main(){
    vec4 pos = xyzw;
    if (!is_transformed) pos = vec4(xyzw.xyz, 1.0);
    gl_Position = proj * view * world * pos;
    fragcol.rgba = color.bgra; //convert from D3D color BGRA (ARGB as little endian) -> RGBA
    ftexcoord = vec2(texcoord.x, 1 - texcoord.y); //TODO: d3d->ogl conversion
})";

const char* fragShader =
R"(#version 330 core
in vec4 fragcol;
in vec2 ftexcoord;
out vec4 color;
uniform sampler2D tex; //this will become an array in the future
struct mat
{
    vec3 ambi;
    vec3 diff;
    vec3 spcl;
    float spcl_strength;
    vec3 emis;
}; //!!TODO
struct dirlight
{
    vec3 ambi;
    vec3 diff;
    vec3 spcl;
    vec3 dir;
}; //!!TODO
struct pointlight
{
    vec3 ambi;
    vec3 diff;
    vec3 spcl;
    vec3 pos;
    float a0;
    float a1;
    float a2;
}; //!!TODO
void main(){
    //color=fragcol;
    //color=vec4(sin(ftexcoord.x), cos(ftexcoord.y), sin(ftexcoord.y), 1);
    color = fragcol * texture(tex, ftexcoord);
})";

CKGLRasterizerContext::CKGLRasterizerContext()
{
}

CKGLRasterizerContext::~CKGLRasterizerContext()
{
    glDeleteProgram(m_CurrentProgram);
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;
    m_DirtyRects.Clear();
    m_PixelShaders.Clear();
    m_VertexShaders.Clear();
    m_IndexBuffers.Clear();
    m_VertexBuffers.Clear();
    m_Sprites.Clear();
    m_Textures.Clear();
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
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
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
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
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
    MessageBoxA(NULL, (char*)glGetString(GL_VERSION), 
        (char*)glGetString(GL_VENDOR), NULL);
    m_Height = Height;
    m_Width = Width;
    m_Bpp = Bpp;
    m_Fullscreen = Fullscreen;
    m_RefreshRate = RefreshRate;
    m_ZBpp = Zbpp;
    m_StencilBpp = StencilBpp;

    if (m_Fullscreen)
        m_Driver->m_Owner->m_FullscreenContext = this;

    m_IndexBuffer = nullptr;

    // TODO: Shader compilation and binding may be moved elsewhere
    CKDWORD vs_idx = 0, ps_idx = 1;
    CKVertexShaderDesc vs_desc;
    vs_desc.m_Function = (CKDWORD*)vertexShader;
    vs_desc.m_FunctionSize = strlen(vertexShader);
    CreateObject(vs_idx, CKRST_OBJ_VERTEXSHADER, &vs_desc);
    CKPixelShaderDesc ps_desc;
    ps_desc.m_Function = (CKDWORD*)fragShader;
    ps_desc.m_FunctionSize = strlen(fragShader);
    CreateObject(ps_idx, CKRST_OBJ_PIXELSHADER, &ps_desc);
    m_CurrentVertexShader = vs_idx;
    m_CurrentPixelShader = ps_idx;
    CKGLVertexShaderDesc* vs = static_cast<CKGLVertexShaderDesc *>(m_VertexShaders[m_CurrentVertexShader]);
    CKGLPixelShaderDesc* ps = static_cast<CKGLPixelShaderDesc*>(m_PixelShaders[m_CurrentPixelShader]);
    m_CurrentProgram = glCreateProgram();
    GLCall(glAttachShader(m_CurrentProgram, vs->GLShader));
    GLCall(glAttachShader(m_CurrentProgram, ps->GLShader));
    GLCall(glLinkProgram(m_CurrentProgram));
    GLCall(glValidateProgram(m_CurrentProgram));
    GLCall(glUseProgram(m_CurrentProgram));
    m_UniformLocationCache["tex"] = glGetUniformLocation(m_CurrentProgram, "tex");
    GLCall(glUniform1i(m_UniformLocationCache["tex"], 0));
    GLCall(glActiveTexture(GL_TEXTURE0));
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
    GLCall(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GLCall(glClearDepth(Z));
    GLCall(glClearStencil(Stencil));
    GLCall(glClear(mask));
    return 1;
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
    SwapBuffers(m_DC);

    return 1;
}

CKBOOL CKGLRasterizerContext::BeginScene()
{
    /*VxMatrix Mat = VxMatrix::Identity();
    SetUniformMatrix4fv("world", 1, GL_FALSE, (float*)&Mat);
    SetUniformMatrix4fv("view", 1, GL_FALSE, (float*)&Mat);
    SetUniformMatrix4fv("proj", 1, GL_FALSE, (float*)&Mat);
    m_WorldMatrix = Mat;
    m_ViewMatrix = Mat;
    m_ProjectionMatrix = Mat;*/
    return 1;
}

CKBOOL CKGLRasterizerContext::EndScene()
{
    //GLCall(glEnd());
    return 1;
}

CKBOOL CKGLRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    //glEnable(GL_LIGHTING);
    assert(glGetError() == GL_NO_ERROR);
    return glGetError() == GL_NO_ERROR;
}

CKBOOL CKGLRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    return CKRasterizerContext::SetMaterial(mat);
}

CKBOOL CKGLRasterizerContext::SetViewport(CKViewportData *data)
{
    
    //glViewport(data->ViewX, data->ViewY, data->ViewWidth, data->ViewHeight);
    //assert(glGetError() == GL_NO_ERROR);
    //glDepthRange(data->ViewZMin, data->ViewZMax);
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
            SetUniformMatrix4fv("world", 1, GL_FALSE, (float*)&Mat);
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            break;
        case VXMATRIX_VIEW:
            m_ViewMatrix = Mat;
            UnityMatrixMask = VIEW_TRANSFORM;
            //Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            SetUniformMatrix4fv("view", 1, GL_FALSE, (float*)&Mat);
            m_MatrixUptodate = 0;
            break;
        case VXMATRIX_PROJECTION:
            m_ProjectionMatrix = Mat;
            UnityMatrixMask = PROJ_TRANSFORM;
            SetUniformMatrix4fv("proj", 1, GL_FALSE, (float*)&Mat);
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
    static CKDWORD oldst[VXRENDERSTATE_MAXSTATE] = {~0U};
    if (oldst[State] != Value)
    {
        oldst[State] = Value;
        return _SetRenderState(State, Value);
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::_SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    static GLenum previous_zfunc = GL_INVALID_ENUM;
    static GLenum srcblend = GL_ONE;
    static GLenum dstblend = GL_ZERO;
    static CKDWORD prev_cull = 0;
    auto vxblend2glblfactor = [](VXBLEND_MODE mode) -> GLenum
    {
        switch (mode)
        {
            case VXBLEND_ZERO: return GL_ZERO;
            case VXBLEND_ONE: return GL_ONE;
            case VXBLEND_SRCCOLOR: return GL_SRC_COLOR;
            case VXBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
            case VXBLEND_SRCALPHA: return GL_SRC_ALPHA;
            case VXBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
            case VXBLEND_DESTALPHA: return GL_DST_ALPHA;
            case VXBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
            case VXBLEND_DESTCOLOR: return GL_DST_COLOR;
            case VXBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
            case VXBLEND_SRCALPHASAT: return GL_SRC_ALPHA_SATURATE;
            case VXBLEND_BOTHSRCALPHA: return GL_CONSTANT_ALPHA;
            case VXBLEND_BOTHINVSRCALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
        }
        return GL_INVALID_ENUM;
    };
    switch (State)
    {
        case VXRENDERSTATE_ZENABLE:
            if (Value)
            {
                if (previous_zfunc != GL_INVALID_ENUM)
                    GLCall(glDepthFunc(previous_zfunc))
            }
            else
                GLCall(glDepthFunc(GL_ALWAYS))
            return TRUE;
        case VXRENDERSTATE_ZWRITEENABLE:
            if (Value)
                GLCall(glEnable(GL_DEPTH_TEST))
            else
                GLCall(glDisable(GL_DEPTH_TEST))
            return TRUE;
        case VXRENDERSTATE_ZFUNC:
            switch (Value)
            {
                case VXCMP_NEVER       : previous_zfunc = GL_NEVER;    break;
                case VXCMP_LESS        : previous_zfunc = GL_LESS;     break;
                case VXCMP_EQUAL       : previous_zfunc = GL_EQUAL;    break;
                case VXCMP_LESSEQUAL   : previous_zfunc = GL_LEQUAL;   break;
                case VXCMP_GREATER     : previous_zfunc = GL_GREATER;  break;
                case VXCMP_NOTEQUAL    : previous_zfunc = GL_NOTEQUAL; break;
                case VXCMP_GREATEREQUAL: previous_zfunc = GL_GEQUAL;   break;
                case VXCMP_ALWAYS      : previous_zfunc = GL_ALWAYS;   break;
            }
            GLCall(glDepthFunc(previous_zfunc));
            return TRUE;
        case VXRENDERSTATE_ALPHABLENDENABLE:
            if (Value)
                GLCall(glEnable(GL_BLEND))
            else
                GLCall(glDisable(GL_BLEND))
            return TRUE;
        case VXRENDERSTATE_BLENDOP:
        {
            GLenum beq = GL_INVALID_ENUM;
            switch(Value)
            {
                case VXBLENDOP_ADD: beq = GL_FUNC_ADD; break;
                case VXBLENDOP_SUBTRACT: beq = GL_FUNC_SUBTRACT; break;
                case VXBLENDOP_REVSUBTRACT: beq = GL_FUNC_REVERSE_SUBTRACT; break;
                case VXBLENDOP_MIN: beq = GL_MIN; break;
                case VXBLENDOP_MAX: beq = GL_MAX; break;
            }
            GLCall(glBlendEquation(beq));
            return TRUE;
        }
        case VXRENDERSTATE_SRCBLEND:
        {
            srcblend = vxblend2glblfactor((VXBLEND_MODE)Value);
            GLCall(glBlendFunc(srcblend, dstblend));
            return TRUE;
        }
        case VXRENDERSTATE_DESTBLEND:
        {
            dstblend = vxblend2glblfactor((VXBLEND_MODE)Value);
            GLCall(glBlendFunc(srcblend, dstblend));
            return TRUE;
        }
        case VXRENDERSTATE_INVERSEWINDING:
        {
            m_InverseWinding = (Value != 0);
            return TRUE;
        }
        case VXRENDERSTATE_CULLMODE:
        {
            DWORD cull_combo = m_InverseWinding << 4 | Value;
            if (cull_combo == prev_cull)
                return TRUE;
            switch (Value)
            {
                case VXCULL_NONE:
                    GLCall(glDisable(GL_CULL_FACE));
                    break;
                case VXCULL_CCW:
                    GLCall(glEnable(GL_CULL_FACE));
                    GLCall(glFrontFace(GL_CW));
                    GLCall(glCullFace(m_InverseWinding ? GL_FRONT : GL_BACK));
                    break;
                case VXCULL_CW:
                    GLCall(glEnable(GL_CULL_FACE));
                    GLCall(glFrontFace(GL_CCW));
                    GLCall(glCullFace(m_InverseWinding ? GL_FRONT : GL_BACK));
                    break;
            }
            prev_cull = cull_combo;
            return TRUE;
        }
    }
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
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKGLTextureDesc *desc = static_cast<CKGLTextureDesc *>(m_Textures[Texture]);
    if (!desc)
    {
        GLCall(glBindTexture(GL_TEXTURE_2D, 0));
        return TRUE;
    }
    desc->Bind(this);
    return TRUE;
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
    fprintf(stderr, "drawprimitive ib %p %d\n", indices, indexcount);
#endif
#if LOG_BATCHSTATS
    ++directbat;
#endif
    CKBOOL clip = 0;
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    if ((data->Flags & CKRST_DP_DOCLIP))
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 1);
        clip = 1;
    } else
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 0);
    }
    CKDWORD VB = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    CKGLVertexBufferDesc *vbo = static_cast<CKGLVertexBufferDesc *>(
        m_VertexBuffers[VB]);
    m_CurrentVertexBuffer = VB;
    vbo->Bind(this);
    void *pbData = nullptr;
    CKDWORD vbase = 0;
    if (vbo->m_CurrentVCount + data->VertexCount <= vbo->m_MaxVertexCount)
    {
        pbData = vbo->Lock(vertexSize * vbo->m_CurrentVCount,
                           vertexSize * data->VertexCount, false);
        vbase = vbo->m_CurrentVCount;
        vbo->m_CurrentVCount += data->VertexCount;
    } else
    {
        pbData = vbo->Lock(0, vertexSize * data->VertexCount, true);
        vbo->m_CurrentVCount = data->VertexCount;
    }
    CKRSTLoadVertexBuffer(static_cast<CKBYTE *>(pbData), vertexFormat, vertexSize, data);
    vbo->Unlock();
    return InternalDrawPrimitive(pType, vbo, vbase, data->VertexCount, indices, indexcount);
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
    return InternalDrawPrimitive(pType, static_cast<CKGLVertexBufferDesc*>(m_VertexBuffers[VertexBuffer]),
        StartIndex, VertexCount, indices, indexcount);
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

CKBOOL CKGLRasterizerContext::InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKGLVertexBufferDesc *vbo, CKDWORD vbase, CKDWORD vcnt, WORD* idx, GLuint icnt)
{
    if (!vbo) return FALSE;
    vbo->Bind(this);

    int ibbase = 0;
    if (idx)
    {
        void *pdata = nullptr;
        if (!m_IndexBuffer || m_IndexBuffer->m_MaxIndexCount < icnt)
        {
            if (m_IndexBuffer) delete m_IndexBuffer;
            m_IndexBuffer = new CKGLIndexBufferDesc();
            m_IndexBuffer->m_MaxIndexCount = icnt + 100;
            m_IndexBuffer->m_CurrentICount = 0;
            m_IndexBuffer->Create();
        }
        m_IndexBuffer->Bind();
        //m_IndexBuffer->m_CurrentICount = 0;
        if (icnt + m_IndexBuffer->m_CurrentICount <= m_IndexBuffer->m_MaxIndexCount)
        {
            pdata = m_IndexBuffer->Lock(2 * m_IndexBuffer->m_CurrentICount, 2 * icnt, false);
            ibbase = 2 * m_IndexBuffer->m_CurrentICount;
            m_IndexBuffer->m_CurrentICount += icnt;
        } else
        {
            pdata = m_IndexBuffer->Lock(0, 2 * icnt, true);
            m_IndexBuffer->m_CurrentICount = icnt;
        }
        if (pdata)
        {
            memcpy(pdata, idx, 2 * icnt);
        }
        m_IndexBuffer->Unlock();
    }

    GLenum glpt = GL_NONE;
    switch (pType)
    {
        case VX_LINELIST:
            glpt = GL_LINES;
            break;
        case VX_LINESTRIP:
            glpt = GL_LINE_STRIP;
            break;
        case VX_TRIANGLELIST:
            glpt = GL_TRIANGLES;
            break;
        case VX_TRIANGLESTRIP:
            glpt = GL_TRIANGLE_STRIP;
            break;
        case VX_TRIANGLEFAN:
            glpt = GL_TRIANGLE_FAN;
            break;
        default:
            break;
    }
    if (idx)
    {
        GLCall(glDrawElementsBaseVertex(glpt, icnt, GL_UNSIGNED_SHORT, (void*)ibbase, vbase));
    }
    else
        GLCall(glDrawArrays(glpt, vbase, vcnt));
    return 1;
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
    if (VB >= m_VertexBuffers.Size()) return NULL;
    CKGLVertexBufferDesc *vbo = static_cast<CKGLVertexBufferDesc*>(m_VertexBuffers[VB]);
    if (!vbo) return NULL;
    return vbo->Lock(StartVertex * vbo->m_VertexSize, VertexCount * vbo->m_VertexSize, (Lock & CKRST_LOCK_NOOVERWRITE) == 0);
}

CKBOOL CKGLRasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    if (VB >= m_VertexBuffers.Size()) return FALSE;
    CKGLVertexBufferDesc *vbo = static_cast<CKGLVertexBufferDesc*>(m_VertexBuffers[VB]);
    if (!vbo) return FALSE;
    vbo->Unlock();
    return TRUE;
}

CKBOOL CKGLRasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKGLTextureDesc *desc = static_cast<CKGLTextureDesc *>(m_Textures[Texture]);
    if (!desc)
        return FALSE;
    VxImageDescEx dst;
    dst.Size = sizeof(VxImageDescEx);
    ZeroMemory(&dst.Flags, sizeof(VxImageDescEx) - sizeof(dst.Size));
    dst.Width = SurfDesc.Width;
    dst.Height = SurfDesc.Height;
    dst.BitsPerPixel = 32;
    dst.BytesPerLine = 4 * SurfDesc.Width;
    dst.AlphaMask = 0xFF000000;
    dst.RedMask = 0x0000FF;
    dst.GreenMask = 0x00FF00;
    dst.BlueMask = 0xFF0000;
    dst.Image = new uint8_t[dst.Width * dst.Height * (dst.BitsPerPixel / 8)];
    VxDoBlitUpsideDown(SurfDesc, dst);
    if (!(SurfDesc.AlphaMask || SurfDesc.Flags >= _DXT1)) VxDoAlphaBlit(dst, 255);
    desc->Bind(this);
    desc->Load(dst.Image);
    delete dst.Image;
    return TRUE;
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
    return CKRasterizerContext::UnlockIndexBuffer(IB); }

unsigned CKGLRasterizerContext::get_shader_location(CKDWORD component) {
    const static std::unordered_map<CKDWORD, unsigned> loc = {
        {CKRST_VF_POSITION, 0},
        {CKRST_VF_RASTERPOS, 0},
        {CKRST_VF_NORMAL, 1},
        {CKRST_VF_DIFFUSE, 2},
        {CKRST_VF_SPECULAR, 3},
        {CKRST_VF_TEX1, 4}
    };
    return loc.find(component) != loc.end() ? loc.find(component)->second : ~0;
}

void CKGLRasterizerContext::set_position_transformed(bool transformed) {
    if (m_UniformLocationCache.find("is_transformed") == m_UniformLocationCache.end())
        m_UniformLocationCache["is_transformed"] = glGetUniformLocation(m_CurrentProgram, "is_transformed");
    glUniform1i(m_UniformLocationCache["is_transformed"], transformed);
}


BOOL CKGLRasterizerContext::SetUniformMatrix4fv(std::string name, GLsizei count, GLboolean transpose,
                                                const GLfloat *value)
{
    int location = 0;
    if (auto it = m_UniformLocationCache.find(name); it != m_UniformLocationCache.end())
        location = it->second;
    else if (m_CurrentProgram == 0)
        return 0;
    else
    {
        location = glGetUniformLocation(m_CurrentProgram, name.c_str());
        m_UniformLocationCache[name] = location;
    }
    GLCall(glUniformMatrix4fv(location, count, transpose, value));
    return 1;
}

CKDWORD CKGLRasterizerContext::GetStaticIndexBuffer(CKDWORD Count, GLushort *IndexData)
{
    // Use count as index.
    // Since we're using it as a static buffer, it should be disposable.
    int index = Count % m_IndexBuffers.Size();

    CKGLIndexBufferDesc* ib = static_cast<CKGLIndexBufferDesc *>(m_IndexBuffers[index]);

    // If valid and meets our need, use it
    if (ib && ib->m_MaxIndexCount == Count)
        return index;
    // ...if not, dispose it
    delete ib;

    ib = new CKGLIndexBufferDesc;
    ib->m_MaxIndexCount = Count;
    ib->m_CurrentICount = 0;
    ib->Create();
    //ib->Populate(IndexData, Count);
    m_IndexBuffers[index] = ib;
    return index;
}

CKDWORD CKGLRasterizerContext::GetDynamicIndexBuffer(CKDWORD Count, GLushort* IndexData, CKDWORD Index)
{
    /*if ((m_Driver->m_3DCaps.CKRasterizerSpecificCaps & CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER) == 0)
        return 0;

    CKDWORD index = VertexFormat & (CKRST_VF_RASTERPOS | CKRST_VF_NORMAL);
    index |= (VertexFormat & (CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEXMASK)) >> 3;
    index >>= 2;
    index |= AddKey << 7;
    index += 1;
    CKIndexBufferDesc *ib = m_IndexBuffers[index];
    ib->m_MaxIndexCount
    if (!vb || vb->m_MaxVertexCount < VertexCount)
    {
        if (vb)
        {
            delete vb;
            m_VertexBuffers[index] = NULL;
        }

        CKVertexBufferDesc nvb;
        nvb.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        nvb.m_VertexFormat = VertexFormat;
        nvb.m_VertexSize = VertexSize;
        nvb.m_MaxVertexCount = (VertexCount + 100 > DEFAULT_VB_SIZE) ? VertexCount + 100 : DEFAULT_VB_SIZE;
        if (AddKey != 0)
            nvb.m_Flags |= CKRST_VB_SHARED;
        CreateObject(index, CKRST_OBJ_INDEXBUFFER, &nvb);
    }

    return index;*/
    return 0;
}

CKBOOL CKGLRasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
    if (Texture >= m_Textures.Size())
        return FALSE;
    if (m_Textures[Texture])
        return TRUE;
#if LOGGING && LOG_CREATETEXTURE
    fprintf(stderr, "create texture %d %dx%d %x\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height, DesiredFormat->Flags);
#endif

    CKGLTextureDesc *desc = new CKGLTextureDesc(DesiredFormat);
    m_Textures[Texture] = desc;
    desc->Create();
    return TRUE;
}

CKBOOL CKGLRasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
    if (VShader >= m_VertexShaders.Size() || !DesiredFormat)
        return 0;
    delete m_VertexShaders[VShader];
    CKGLVertexShaderDesc* desc = new CKGLVertexShaderDesc;
    DesiredFormat->m_Function = (CKDWORD*)vertexShader;
    DesiredFormat->m_FunctionSize = strlen(vertexShader);
    if (desc->Create(this, DesiredFormat))
    {
        m_VertexShaders[VShader] = desc;
        return 1;
    }
    return 0;
}

CKBOOL CKGLRasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return 0;
    delete m_PixelShaders[PShader];
    CKGLPixelShaderDesc* desc = new CKGLPixelShaderDesc;
    DesiredFormat->m_Function = (CKDWORD*)fragShader;
    DesiredFormat->m_FunctionSize = strlen(fragShader);
    if (desc->Create(this, DesiredFormat))
    {
        m_PixelShaders[PShader] = desc;
        return 1;
    }
    return 0;
}

CKBOOL CKGLRasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return 0;
    delete m_VertexBuffers[VB];

    CKGLVertexBufferDesc* desc = new CKGLVertexBufferDesc(DesiredFormat);
    desc->Create();
    m_VertexBuffers[VB] = desc;
#if LOGGING && LOG_CREATEBUFFER
    fprintf(stderr, "\rvbo avail:");
    for (int i = 0; i < m_VertexBuffers.Size(); ++i)
    {
        if (m_VertexBuffers[i])
            fprintf(stderr, " %d", i);
    }
#endif
    return 1;
}

CKBOOL CKGLRasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return 0;
    delete m_IndexBuffers[IB];
    CKGLIndexBufferDesc* desc = new CKGLIndexBufferDesc(DesiredFormat);
    desc->Create();
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
