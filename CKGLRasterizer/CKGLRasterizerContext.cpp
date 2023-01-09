#include "CKGLRasterizer.h"
#define LOGGING 0
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

VxMatrix inv(const VxMatrix &m);

static HMODULE self_module = nullptr;

static HMODULE get_self_module()
{
    if (!self_module)
    {
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (char*)&self_module, &self_module);
    }
    return self_module;
}

CKDWORD get_resource_size(const char* type, const char* name)
{
    HRSRC rsc = FindResourceA(get_self_module(), name, type);
    if (!rsc) return 0;
    return SizeofResource(get_self_module(), rsc);
}

void* get_resource_data(const char* type, const char* name)
{
    HRSRC rsc = FindResourceA(get_self_module(), name, type);
    if (!rsc) return nullptr;
    HGLOBAL hrdat = LoadResource(get_self_module(), rsc);
    if (!hrdat) return nullptr;
    return LockResource(hrdat);
}

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
    m_Window = Window;
    m_orig_title.resize(GetWindowTextLengthA(GetAncestor((HWND)Window, GA_ROOT)) + 1);
    GetWindowTextA(GetAncestor((HWND)Window, GA_ROOT), m_orig_title.data(), m_orig_title.size());
    while (m_orig_title.back() == '\0') m_orig_title.pop_back();
    ShowWindow((HWND)Window, SW_SHOW);
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
    memset(m_renderst, 0xff, sizeof(m_renderst));

    // TODO: Shader compilation and binding may be moved elsewhere
    CKDWORD vs_idx = 0, ps_idx = 1;
    CKVertexShaderDesc vs_desc;
    vs_desc.m_Function = (CKDWORD*)get_resource_data("CKGLR_VERT_SHADER", "BUILTIN_VERTEX_SHADER");
    vs_desc.m_FunctionSize = get_resource_size("CKGLR_VERT_SHADER", "BUILTIN_VERTEX_SHADER");
    CreateObject(vs_idx, CKRST_OBJ_VERTEXSHADER, &vs_desc);
    CKPixelShaderDesc ps_desc;
    ps_desc.m_Function = (CKDWORD*)get_resource_data("CKGLR_FRAG_SHADER", "BUILTIN_FRAGMENT_SHADER");
    ps_desc.m_FunctionSize = get_resource_size("CKGLR_FRAG_SHADER", "BUILTIN_FRAGMENT_SHADER");
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
    GLCall(glUniform1i(get_uniform_location("tex"), 0));
    GLCall(glActiveTexture(GL_TEXTURE0));

    m_lighting_flags = LSW_LIGHTING_ENABLED | LSW_VRTCOLOR_ENABLED;
    GLCall(glUniform1ui(get_uniform_location("lighting_switches"), m_lighting_flags));
    {
        CKTextureDesc blank;
        blank.Format.Width = 1;
        blank.Format.Height = 1;
        CKDWORD white = ~0U;
        CreateTexture(0, &blank);
        CKGLTextureDesc* blanktex = static_cast<CKGLTextureDesc*>(m_Textures[0]);
        blanktex->Bind(this);
        blanktex->Load(&white);
    }
    return TRUE;
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
    {
        GLCall(glDepthMask(true));
        mask |= GL_DEPTH_BUFFER_BIT;
    }
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
#endif
#if LOG_BATCHSTATS
    set_title_status("OpenGL %s | batch stats: direct %d, vb %d, vbib %d", glGetString(GL_VERSION), directbat, vbbat, vbibbat);
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
    return 1;
}

CKBOOL CKGLRasterizerContext::EndScene()
{
    //GLCall(glEnd());
    return 1;
}

CKBOOL CKGLRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    if (Light >= m_lights.size())
        m_lights.resize(Light + 1);
    m_lights[Light].second = *data;
    return TRUE;
}

CKBOOL CKGLRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    if (Light >= m_lights.size())
        return FALSE;
    m_lights[Light].first = Enable;
    //!!FIXME: this implementation is temporary, AND WRONG.
    //only for directional light testing for now.
    auto lit = &m_lights[Light].second;
    if (m_lights[Light].second.Type == VX_LIGHTDIREC)
    {
        glUniform1ui(get_uniform_location("lights.type"), 3);
        glUniform3fv(get_uniform_location("lights.ambi"), 1, lit->Ambient.col);
        glUniform3fv(get_uniform_location("lights.diff"), 1, lit->Diffuse.col);
        glUniform3fv(get_uniform_location("lights.spcl"), 1, lit->Specular.col);
        glUniform3fv(get_uniform_location("lights.dir"), 1, lit->Direction.v);
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    //ignore alpha
    glUniform3fv(get_uniform_location("material.ambi"), 1, mat->Ambient.col);
    glUniform3fv(get_uniform_location("material.diff"), 1, mat->Diffuse.col);
    glUniform3fv(get_uniform_location("material.spcl"), 1, mat->Specular.col);
    glUniform3fv(get_uniform_location("material.emis"), 1, mat->Emissive.col);
    glUniform1f(get_uniform_location("material.spcl_strength"), mat->SpecularPower);
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetViewport(CKViewportData *data)
{
    GLCall(glViewport(data->ViewX, data->ViewY, data->ViewWidth, data->ViewHeight));
    GLCall(glDepthRangef(data->ViewZMin, data->ViewZMax));
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    CKDWORD UnityMatrixMask = 0;
    switch (Type)
    {
        case VXMATRIX_WORLD:
        {
            m_WorldMatrix = Mat;
            UnityMatrixMask = WORLD_TRANSFORM;
            SetUniformMatrix4fv("world", 1, GL_FALSE, (float*)&Mat);
            VxMatrix im,tim;
            Vx3DTransposeMatrix(tim, Mat); //row-major to column-major conversion madness
            im = inv(tim);

            SetUniformMatrix4fv("tiworld", 1, GL_FALSE, (float*)&im);
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            break;
        }
        case VXMATRIX_VIEW:
        {
            m_ViewMatrix = Mat;
            UnityMatrixMask = VIEW_TRANSFORM;
            //Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            SetUniformMatrix4fv("view", 1, GL_FALSE, (float*)&Mat);
            m_MatrixUptodate = 0;
            VxMatrix t;
            Vx3DInverseMatrix(t, Mat);
            m_viewpos = VxVector(t[3][0], t[3][1], t[3][2]);
            GLCall(glUniform3fv(get_uniform_location("vpos"), 1, (float*)&m_viewpos));
            break;
        }
        case VXMATRIX_PROJECTION:
        {
            m_ProjectionMatrix = Mat;
            UnityMatrixMask = PROJ_TRANSFORM;
            SetUniformMatrix4fv("proj", 1, GL_FALSE, (float*)&Mat);
            m_MatrixUptodate = 0;
            break;
        }
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
    if (m_renderst[State] != Value)
    {
        m_renderst[State] = Value;
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
    auto update_depth_switches = [this]() {
        if (m_renderst[VXRENDERSTATE_ZENABLE] || m_renderst[VXRENDERSTATE_ZWRITEENABLE])
        {
            GLCall(glEnable(GL_DEPTH_TEST));
        }
        else
        {
            GLCall(glDisable(GL_DEPTH_TEST));
        }
    };
    auto send_light_switches = [this]() {
        GLCall(glUniform1ui(get_uniform_location("lighting_switches"), m_lighting_flags));
    };
    auto toggle_flag = [](CKDWORD *flags, CKDWORD flag, bool enabled) {
        if (enabled) *flags |= flag;
        else *flags &= ~0U ^ flag;
    };
    switch (State)
    {
        case VXRENDERSTATE_ZENABLE:
            //Double check against Direct3D behavior here.
            //Microsoft's documentation is confusing.
            if (Value)
            {
                if (previous_zfunc != GL_INVALID_ENUM)
                    GLCall(glDepthFunc(previous_zfunc))
            }
            else
                GLCall(glDepthFunc(GL_ALWAYS))
            update_depth_switches();
            return TRUE;
        case VXRENDERSTATE_ZWRITEENABLE:
            GLCall(glDepthMask(Value));
            update_depth_switches();
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
        case VXRENDERSTATE_SPECULARENABLE:
        {
            toggle_flag(&m_lighting_flags, LSW_SPECULAR_ENABLED, Value);
            send_light_switches();
            return TRUE;
        }
        case VXRENDERSTATE_LIGHTING:
        {
            toggle_flag(&m_lighting_flags, LSW_LIGHTING_ENABLED, Value);
            send_light_switches();
            return TRUE;
        }
        case VXRENDERSTATE_COLORVERTEX:
        {
            toggle_flag(&m_lighting_flags, LSW_VRTCOLOR_ENABLED, Value);
            send_light_switches();
            return TRUE;
        }
        //case VXRENDERSTATE_DITHERENABLE:
        //case VXRENDERSTATE_TEXTUREPERSPECTIVE:
        //case VXRENDERSTATE_NORMALIZENORMALS:
        //case VXRENDERSTATE_AMBIENT:
        //case VXRENDERSTATE_ALPHAFUNC:
        //case VXRENDERSTATE_ALPHATESTENABLE:
        //case VXRENDERSTATE_SHADEMODE:
        //case VXRENDERSTATE_FILLMODE:
        //case VXRENDERSTATE_CLIPPING:
        //case VXRENDERSTATE_FOGENABLE:
        //case VXRENDERSTATE_FOGCOLOR:
        //case VXRENDERSTATE_FOGSTART:
        //case VXRENDERSTATE_FOGEND:
        //case VXRENDERSTATE_FOGDENSITY:
        default:
            return FALSE;
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
    //Currently, we ignore the Stage parameter (we only support 1 stage as of yet)
    auto vxaddrmode2glwrap = [](VXTEXTURE_ADDRESSMODE am) -> GLenum
    {
        switch (am)
        {
            case VXTEXTURE_ADDRESSWRAP: return GL_REPEAT;
            case VXTEXTURE_ADDRESSMIRROR: return GL_MIRRORED_REPEAT;
            case VXTEXTURE_ADDRESSCLAMP: return GL_CLAMP_TO_EDGE;
            case VXTEXTURE_ADDRESSBORDER: return GL_CLAMP_TO_BORDER;
            case VXTEXTURE_ADDRESSMIRRORONCE: return GL_MIRROR_CLAMP_TO_EDGE;
            default: return GL_INVALID_ENUM;
        }
    };
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
            GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value)));
            GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value)));
            GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value)));
            break;
        case CKRST_TSS_ADDRESSU:
            GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value)));
            break;
        case CKRST_TSS_ADDRESSV:
            GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value)));
            break;
        case CKRST_TSS_BORDERCOLOR:
        {
            VxColor c(Value);
            GLCall(glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (float*)&c.col));
            break;
        }
        default:
            return FALSE;
    }
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

int CKGLRasterizerContext::get_uniform_location(const char *name)
{
    if (m_CurrentProgram == INVALID_VALUE) return ~0;
    if (m_UniformLocationCache.find(name) == m_UniformLocationCache.end())
        m_UniformLocationCache[name] = glGetUniformLocation(m_CurrentProgram, name);
    return m_UniformLocationCache[name];
}

unsigned CKGLRasterizerContext::get_vertex_attrib_location(CKDWORD component)
{
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
    glUniform1i(get_uniform_location("is_transformed"), transformed);
}

void CKGLRasterizerContext::set_vertex_has_color(bool color) {
    glUniform1i(get_uniform_location("has_color"), color); }

void CKGLRasterizerContext::set_title_status(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list argsx;
    va_copy(argsx, args);
    std::string ts;
    ts.resize(vsnprintf(NULL, 0, fmt, argsx) + 1);
    va_end(argsx);
    vsnprintf(ts.data(), ts.size(), fmt, args);
    va_end(args);

    ts = m_orig_title + " | " + ts;
    SetWindowTextA(GetAncestor((HWND)m_Window, GA_ROOT), ts.c_str());
}


BOOL CKGLRasterizerContext::SetUniformMatrix4fv(std::string name, GLsizei count, GLboolean transpose,
                                                const GLfloat *value)
{
    GLCall(glUniformMatrix4fv(get_uniform_location(name.c_str()), count, transpose, value));
    return TRUE;
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
    desc->m_Function = DesiredFormat->m_Function;
    desc->m_FunctionSize  = DesiredFormat->m_FunctionSize;
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
    desc->m_Function = DesiredFormat->m_Function;
    desc->m_FunctionSize  = DesiredFormat->m_FunctionSize;
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

VxMatrix inv(const VxMatrix &_m)
{
    //taken from https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    float (*m)[4] = (float(*)[4])&_m;
    float A2323 = m[2][2] * m[3][3] - m[2][3] * m[3][2] ;
    float A1323 = m[2][1] * m[3][3] - m[2][3] * m[3][1] ;
    float A1223 = m[2][1] * m[3][2] - m[2][2] * m[3][1] ;
    float A0323 = m[2][0] * m[3][3] - m[2][3] * m[3][0] ;
    float A0223 = m[2][0] * m[3][2] - m[2][2] * m[3][0] ;
    float A0123 = m[2][0] * m[3][1] - m[2][1] * m[3][0] ;
    float A2313 = m[1][2] * m[3][3] - m[1][3] * m[3][2] ;
    float A1313 = m[1][1] * m[3][3] - m[1][3] * m[3][1] ;
    float A1213 = m[1][1] * m[3][2] - m[1][2] * m[3][1] ;
    float A2312 = m[1][2] * m[2][3] - m[1][3] * m[2][2] ;
    float A1312 = m[1][1] * m[2][3] - m[1][3] * m[2][1] ;
    float A1212 = m[1][1] * m[2][2] - m[1][2] * m[2][1] ;
    float A0313 = m[1][0] * m[3][3] - m[1][3] * m[3][0] ;
    float A0213 = m[1][0] * m[3][2] - m[1][2] * m[3][0] ;
    float A0312 = m[1][0] * m[2][3] - m[1][3] * m[2][0] ;
    float A0212 = m[1][0] * m[2][2] - m[1][2] * m[2][0] ;
    float A0113 = m[1][0] * m[3][1] - m[1][1] * m[3][0] ;
    float A0112 = m[1][0] * m[2][1] - m[1][1] * m[2][0] ;

    float det = m[0][0] * ( m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223 )
        - m[0][1] * ( m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223 )
        + m[0][2] * ( m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123 )
        - m[0][3] * ( m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123 ) ;
    det = 1 / det;
    float ret[4][4];
    ret[0][0] = det *   ( m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223 );
    ret[0][1] = det * - ( m[0][1] * A2323 - m[0][2] * A1323 + m[0][3] * A1223 );
    ret[0][2] = det *   ( m[0][1] * A2313 - m[0][2] * A1313 + m[0][3] * A1213 );
    ret[0][3] = det * - ( m[0][1] * A2312 - m[0][2] * A1312 + m[0][3] * A1212 );
    ret[1][0] = det * - ( m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223 );
    ret[1][1] = det *   ( m[0][0] * A2323 - m[0][2] * A0323 + m[0][3] * A0223 );
    ret[1][2] = det * - ( m[0][0] * A2313 - m[0][2] * A0313 + m[0][3] * A0213 );
    ret[1][3] = det *   ( m[0][0] * A2312 - m[0][2] * A0312 + m[0][3] * A0212 );
    ret[2][0] = det *   ( m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123 );
    ret[2][1] = det * - ( m[0][0] * A1323 - m[0][1] * A0323 + m[0][3] * A0123 );
    ret[2][2] = det *   ( m[0][0] * A1313 - m[0][1] * A0313 + m[0][3] * A0113 );
    ret[2][3] = det * - ( m[0][0] * A1312 - m[0][1] * A0312 + m[0][3] * A0112 );
    ret[3][0] = det * - ( m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123 );
    ret[3][1] = det *   ( m[0][0] * A1223 - m[0][1] * A0223 + m[0][2] * A0123 );
    ret[3][2] = det * - ( m[0][0] * A1213 - m[0][1] * A0213 + m[0][2] * A0113 );
    ret[3][3] = det *   ( m[0][0] * A1212 - m[0][1] * A0212 + m[0][2] * A0112 );

    return VxMatrix(ret);
}