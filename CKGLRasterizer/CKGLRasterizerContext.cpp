#include "CKGLRasterizer.h"
#include "CKGLVertexBuffer.h"
#include "CKGLIndexBuffer.h"
#include "CKGLTexture.h"
#include "CKGLProgram.h"
#include "CKGLPostProcessing.h"
#include "EnumMaps.h"

#define LOG_LOADTEXTURE 0
#define LOG_CREATETEXTURE 0
#define LOG_CREATEBUFFER 0
#define LOG_DRAWPRIMITIVE 0
#define LOG_DRAWPRIMITIVEVB 0
#define LOG_DRAWPRIMITIVEVBIB 0
#define LOG_SETTEXTURE 0
#define LOG_FLUSHCACHES 0
#define LOG_RENDERSTATE 0

#define DYNAMIC_VBO_COUNT 64
#define DYNAMIC_IBO_COUNT 64

static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;

extern void debug_setup(CKGLRasterizerContext *rst);

void GLAPIENTRY debug_callback(GLenum src, GLenum typ, GLuint id, GLenum severity,
    GLsizei len, const GLchar *msg, const void *_user)
{
    fprintf(stderr, "OpenGL message: source 0x%x type 0x%x id 0x%x severity 0x%x: %s\n", src, typ, id, severity, msg);
    if (severity == GL_DEBUG_SEVERITY_HIGH)
        MessageBoxA(NULL, msg, "OpenGL Error", 0);
}

VxMatrix inv(const VxMatrix &m);

CKGLRasterizerContext::CKGLRasterizerContext()
{
}

CKGLRasterizerContext::~CKGLRasterizerContext()
{
    if (m_2dpp)
    {
        m_2dpp->clear_stages();
        delete m_2dpp;
    }
    if (m_3dpp)
    {
        m_3dpp->clear_stages();
        delete m_3dpp;
    }
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;

    for (auto dvb : m_dynvbo)
        delete dvb.second;
    m_dynvbo.clear();

    for (auto dib : m_dynibo)
        delete dib.second;
    m_dynibo.clear();

    if (m_prgm)
        delete m_prgm;

    FlushObjects(CKRST_OBJ_ALL);

    m_DirtyRects.Clear();
    m_PixelShaders.Clear();
    m_VertexShaders.Clear();
    m_IndexBuffers.Clear();
    m_VertexBuffers.Clear();
    m_Sprites.Clear();
    m_Textures.Clear();
    ReleaseDC((HWND)m_Window, m_DC);
}

PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
CKBOOL CKGLRasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
    CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
    debug_setup(this);
    HINSTANCE hInstance = GetModuleHandle(NULL);

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
        WGL_SAMPLE_BUFFERS_ARB, m_Antialias ? GL_TRUE : GL_FALSE,
        WGL_SAMPLES_ARB, m_Antialias,
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
    const std::vector<std::pair<int, int>> gl_versions =
        {{4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0},
         {3, 3}, {3, 2}, {3, 1}, {3, 0}};
    HGLRC RC = NULL;
    for (auto ver : gl_versions)
    {
#ifdef GL_DEBUG
        const int context_flag = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
        const int context_flag = 0;
#endif
        int  contextAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, ver.first,
            WGL_CONTEXT_MINOR_VERSION_ARB, ver.second,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB, context_flag,
            0
        };

        RC = wglCreateContextAttribsARB(DC, NULL, contextAttribs);
        if (RC != NULL) break;
    }
    if (RC == NULL)
        return FALSE;
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(fakeRC);
    ReleaseDC(fakeWND, fakeDC);
    DestroyWindow(fakeWND);
    m_Window = Window;
    m_DC = DC;
    if (!wglMakeCurrent(DC, RC))
        return FALSE;
    if (glewInit() != GLEW_OK)
        return FALSE;
    //check GL version and required extensions...
    //required extension                core since
    //ARB_explicit_attrib_location      3.3
    //ARB_texture_storage               4.2
    //ARB_vertex_attrib_binding         4.3
    //ARB_direct_state_access           4.5
    //!!TODO: also needs checking: >=4.2 || ARB_texture_storage
    if (!( GLEW_VERSION_4_5 ||
          (GLEW_VERSION_4_3 && GLEW_ARB_direct_state_access) ||
          (GLEW_VERSION_3_3 && GLEW_ARB_direct_state_access && GLEW_ARB_vertex_attrib_binding)))
    {
        std::string glver = std::string((char*)glGetString(GL_VERSION));
        int response = MessageBoxA((HWND)m_Window,
            ("Unsupported OpenGL version (" + glver + "). Do you want to try using this context anyway?").c_str(),
            "Cannot start rasterizer context",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (response == IDNO)
        {
            wglDeleteContext(RC);
            ReleaseDC((HWND)Window, DC);
            return FALSE;
        }
    }
#ifdef GL_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debug_callback, nullptr);
#endif
    ULONG_PTR style = GetClassLongPtr((HWND)Window, GCL_STYLE);
    style &= (~CS_VREDRAW);
    style &= (~CS_HREDRAW);
    SetClassLongPtr((HWND)Window, GCL_STYLE, style);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SetClassLongPtr((HWND)Window, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
    wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(m_Vsync ? 1 : 0);
    TracyGpuContext;

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

    memset(m_renderst, 0xff, sizeof(m_renderst));
    memset(m_lights_data, 0, sizeof(m_lights_data));

    // TODO: Shader compilation and binding may be moved elsewhere
    m_prgm = new CKGLProgram(load_resource("CKGLR_VERT_SHADER", "BUILTIN_VERTEX_SHADER"),
                             load_resource("CKGLR_FRAG_SHADER", "BUILTIN_FRAGMENT_SHADER"));
    m_prgm->validate();
    m_prgm->use();
    for (int i = 0; i < CKRST_MAX_STAGES; ++i)
        m_prgm->stage_uniform("tex[" + std::to_string(i) + "]", CKGLUniformValue::make_i32(i));

    m_lighting_flags = LSW_LIGHTING_ENABLED;
    m_prgm->stage_uniform("lighting_switches", CKGLUniformValue::make_u32v(1, &m_lighting_flags));
    m_alpha_test_flags = 8; //alpha test off, alpha function always
    m_prgm->stage_uniform("alphatest_flags", CKGLUniformValue::make_u32v(1, &m_alpha_test_flags));
    m_fog_flags = 0; //fog off, fog type none. We do not support vertex fog.
    VxColor init_fog_color = VxColor();
    m_fog_parameters[0] = 0.;
    m_fog_parameters[1] = 1.;
    m_fog_parameters[2] = 1.;
    m_prgm->stage_uniform("fog_flags", CKGLUniformValue::make_u32v(1, &m_fog_flags));
    m_prgm->stage_uniform("fog_color", CKGLUniformValue::make_f32v4v(1, (float*)&init_fog_color.col, true));
    m_prgm->stage_uniform("fog_parameters", CKGLUniformValue::make_f32v3v(1, (float*)&m_fog_parameters));
    m_null_texture_mask = (1 << (CKRST_MAX_STAGES + 1)) - 1;
    m_prgm->stage_uniform("null_texture_mask", CKGLUniformValue::make_u32v(1, &m_null_texture_mask));
    m_Textures[0] = new CKGLTexture(); //still need this because I'm too lazy to handle them in SetTextureStageState...

    //setup material uniform block
    m_prgm->define_uniform_block("MatUniformBlock", 16, sizeof(CKGLMaterialUniform), nullptr);
    //setup lights uniform block
    m_prgm->define_uniform_block("LightsUniformBlock", 16, MAX_ACTIVE_LIGHTS * sizeof(CKGLLightUniform), m_lights_data);
    //initialize texture combinator stuff
    m_texcombo[0] = CKGLTexCombinatorUniform::make(
        TexOp::modulate, TexArg::texture, TexArg::current, TexArg::current,
        TexOp::select1, TexArg::diffuse, TexArg::current, TexArg::current,
        TexArg::current, ~0U);
    for (int i = 0 ; i < 7; ++i)
        m_texcombo[1] = CKGLTexCombinatorUniform::make(
            TexOp::disable, TexArg::texture, TexArg::current, TexArg::current,
            TexOp::disable, TexArg::diffuse, TexArg::current, TexArg::current,
            TexArg::current, ~0U);
    //setup texture combinator uniform block
    m_prgm->define_uniform_block("TexCombinatorUniformBlock", 16, CKRST_MAX_STAGES * sizeof(CKGLTexCombinatorUniform), m_texcombo);

    for (int i = 0; i < CKRST_MAX_STAGES; ++i)
        m_prgm->stage_uniform("texp[" + std::to_string(i) + "]", CKGLUniformValue::make_u32v(1, (uint32_t*)&m_tex_vp[i]));

    m_ProjectionMatrix = VxMatrix::Identity();
    m_ViewMatrix = VxMatrix::Identity();
    m_WorldMatrix = VxMatrix::Identity();
    m_ModelViewMatrix = VxMatrix::Identity();
    m_2dvpmtx = VxMatrix::Identity();
    m_tiworldmtx = VxMatrix::Identity();
    m_tiworldviewmtx = VxMatrix::Identity();
    for (int i = 0; i < CKRST_MAX_STAGES; ++i)
        m_textrmtx[i] = VxMatrix::Identity();
    m_prgm->stage_uniform("proj", CKGLUniformValue::make_f32mat4(1, (float*)&m_ProjectionMatrix));
    m_prgm->stage_uniform("view", CKGLUniformValue::make_f32mat4(1, (float*)&m_ViewMatrix));
    m_prgm->stage_uniform("world", CKGLUniformValue::make_f32mat4(1, (float*)&m_WorldMatrix));
    m_prgm->stage_uniform("tiworld", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldmtx));
    m_prgm->stage_uniform("tiworldview", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldviewmtx));
    m_prgm->stage_uniform("mvp2d", CKGLUniformValue::make_f32mat4(1, (float*)&m_2dvpmtx));
    m_prgm->stage_uniform("textr", CKGLUniformValue::make_f32mat4(CKRST_MAX_STAGES, (float*)&m_textrmtx[0]));

    set_position_transformed(true);
    set_vertex_has_color(true);
    m_prgm->send_uniform();

    m_renderst[VXRENDERSTATE_SRCBLEND] = VXBLEND_ONE;
    m_renderst[VXRENDERSTATE_DESTBLEND] = VXBLEND_ZERO;
    m_renderst[VXRENDERSTATE_COLORVERTEX] = FALSE;

    for (m_max_ppsh_id = 1; m_max_ppsh_id < 256 && get_resource_size("CKGLRPP_DESC", (char*)m_max_ppsh_id) != 0; ++m_max_ppsh_id);
    if (m_max_ppsh_id > 255) m_max_ppsh_id = 1;
    m_current_ppsh_id = 1;
    m_3dpp = new CKGLPostProcessingPipeline();
    m_2dpp = new CKGLPostProcessingPipeline();
    m_3dpp->parse_pipeline_config(load_resource("CKGLRPP_DESC", (char*)m_current_ppsh_id));
    m_2dpp->parse_pipeline_config(load_resource("CKGLRPP_DESC", (char*)1));
    m_3dpp->setup_fbo(true, true, m_Width, m_Height);
    m_2dpp->setup_fbo(false, false, m_Width, m_Height);
    return TRUE;
}

CKBOOL CKGLRasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    m_Height = Height;
    m_Width = Width;
    m_3dpp->setup_fbo(true, true, m_Width, m_Height);
    m_2dpp->setup_fbo(false, false, m_Width, m_Height);
    return TRUE;
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
        glDepthMask(true);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_use_post_processing)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    VxColor c(Ccol);
    glClearColor(c.r, c.g, c.b, c.a);
    glClearDepth(Z);
    glClearStencil(Stencil);
    glClear(mask);
    if (m_step_mode == 2)
    {
        BackToFront(FALSE);
        glClear(mask);
        BackToFront(FALSE);
    }
    if (m_use_post_processing)
    {
        m_3dpp->set_as_target();
        glClear(mask);
        m_2dpp->set_as_target();
        glClearColor(c.r, c.g, c.b, 0);
        glClear(mask);
    }
    _SetRenderState(VXRENDERSTATE_ZWRITEENABLE, m_renderst[VXRENDERSTATE_ZWRITEENABLE]);
    return 1;
}

CKBOOL CKGLRasterizerContext::BackToFront(CKBOOL vsync)
{
    if (m_batch_status)
    {
        if (m_use_post_processing)
            set_title_status("OpenGL %s | batch stats: direct %d, vb %d, vbib %d | post processing %s", glGetString(GL_VERSION), directbat, vbbat, vbibbat, m_3dpp->get_name().c_str());
        else
            set_title_status("OpenGL %s | batch stats: direct %d, vb %d, vbib %d", glGetString(GL_VERSION), directbat, vbbat, vbibbat);
    }
    TracyPlot("DirectBatch", (int64_t)directbat);
    TracyPlot("VB", (int64_t)vbbat);
    TracyPlot("VBIB", (int64_t)vbibbat);
    directbat = 0;
    vbbat = 0;
    vbibbat = 0;
    if (vsync != m_Vsync && wglSwapIntervalEXT)
    {
        m_Vsync = vsync;
        wglSwapIntervalEXT(vsync ? 1 : 0);
    }

    if (m_use_post_processing)
    {
        m_target_mode = 0;
        m_current_vf = ~0U;
        if (!m_renderst[VXRENDERSTATE_ALPHABLENDENABLE])
            _SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        if (m_renderst[VXRENDERSTATE_ZENABLE])
            _SetRenderState(VXRENDERSTATE_ZENABLE, FALSE);
        if (m_renderst[VXRENDERSTATE_CULLMODE] != VXCULL_NONE)
            _SetRenderState(VXRENDERSTATE_ZENABLE, VXCULL_NONE);
        if (m_renderst[VXRENDERSTATE_FILLMODE] != VXFILL_SOLID)
            _SetRenderState(VXRENDERSTATE_ZENABLE, VXFILL_SOLID);
        //here we are expecting:
        // blending on; 1,1-srcalpha
        // depth testing disabled or always passes
        // face culling disabled
        // solid polygon filling

        m_3dpp->draw();
        if (m_2d_enabled)
            m_2dpp->draw();
        m_prgm->use();

        //restore to whatever the state was before
        if (!m_renderst[VXRENDERSTATE_ALPHABLENDENABLE])
            _SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, FALSE);
        _SetRenderState(VXRENDERSTATE_SRCBLEND, m_renderst[VXRENDERSTATE_SRCBLEND]);
        _SetRenderState(VXRENDERSTATE_DESTBLEND, m_renderst[VXRENDERSTATE_DESTBLEND]);
        if (m_renderst[VXRENDERSTATE_ZENABLE])
            _SetRenderState(VXRENDERSTATE_ZENABLE, TRUE);
        if (m_renderst[VXRENDERSTATE_CULLMODE] != VXCULL_NONE)
            _SetRenderState(VXRENDERSTATE_CULLMODE, m_renderst[VXRENDERSTATE_CULLMODE]);
        if (m_renderst[VXRENDERSTATE_FILLMODE] != VXFILL_SOLID)
            _SetRenderState(VXRENDERSTATE_ZENABLE, m_renderst[VXRENDERSTATE_FILLMODE]);
        m_cur_ts = ~0U;
        memset(m_ts_texture, 0xFF, sizeof(m_ts_texture));

        if (m_ppsh_switch_pending)
        {
            if (++m_current_ppsh_id >= m_max_ppsh_id)
                m_current_ppsh_id = 1;
            if (m_3dpp) delete m_3dpp;
            m_3dpp = new CKGLPostProcessingPipeline();
            m_3dpp->parse_pipeline_config(load_resource("CKGLRPP_DESC", (char*)m_current_ppsh_id));
            m_3dpp->setup_fbo(true, true, m_Width, m_Height);
            m_ppsh_switch_pending = false;
        }
    }

    if (m_use_pp_switch_pending)
    {
        m_use_post_processing = !m_use_post_processing;
        m_use_pp_switch_pending = false;
    }

    SwapBuffers(m_DC);
    TracyGpuCollect;
    if (m_step_mode == 1)
    {
        set_title_status("stepping frame | X in console = quit step mode | Any key = next frame");
        step_mode_wait();
    }

    return 1;
}

CKBOOL CKGLRasterizerContext::BeginScene()
{
    FrameMark;
    return 1;
}

CKBOOL CKGLRasterizerContext::EndScene()
{
    return 1;
}

CKBOOL CKGLRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    ZoneScopedN(__FUNCTION__);
    if (Light < RST_MAX_LIGHT)
        m_CurrentLightData[Light] = *data;
    if (Light >= m_lights.size())
        m_lights.resize(Light + 1, std::pair<CKDWORD, CKLightData>(MAX_ACTIVE_LIGHTS, CKLightData()));
    m_lights[Light].second = *data;
    if (m_lights[Light].first < MAX_ACTIVE_LIGHTS)
    {
        m_lights_data[m_lights[Light].first] = CKGLLightUniform(m_lights[Light].second);
        //m_prgm->update_uniform_block("LightsUniformBlock", m_lights[Light].first * sizeof(CKGLLightUniform), sizeof(CKGLLightUniform), &m_lights_data[m_lights[Light].first]);
        m_prgm->update_uniform_block("LightsUniformBlock", 0, MAX_ACTIVE_LIGHTS * sizeof(CKGLLightUniform) , m_lights_data);
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    ZoneScopedN(__FUNCTION__);
    if (Light >= m_lights.size())
        return FALSE;
    if (!((m_lights[Light].first < MAX_ACTIVE_LIGHTS) ^ Enable))
        return TRUE;
    if (Enable)
    {
        if (m_lights[Light].first >= MAX_ACTIVE_LIGHTS)
        {
            CKDWORD &i = m_lights[Light].first;
            for (i = 0; i < MAX_ACTIVE_LIGHTS && m_lights_data[i].type != 0; ++i);
        }
        if (m_lights[Light].first >= MAX_ACTIVE_LIGHTS)
            return FALSE;
        m_lights_data[m_lights[Light].first] = CKGLLightUniform(m_lights[Light].second);
    }
    else
    {
        if (m_lights[Light].first < MAX_ACTIVE_LIGHTS)
            m_lights_data[m_lights[Light].first].type = 0;
    }
    //m_prgm->update_uniform_block("LightsUniformBlock", m_lights[Light].first * sizeof(CKGLLightUniform), sizeof(CKGLLightUniform), &m_lights_data[m_lights[Light].first]);
    m_prgm->update_uniform_block("LightsUniformBlock", 0, MAX_ACTIVE_LIGHTS * sizeof(CKGLLightUniform) , m_lights_data);
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    ZoneScopedN(__FUNCTION__);
    //DX9Rasterizer checks mat for null pointer... can mat be null?
    m_CurrentMaterialData = *mat;
    CKGLMaterialUniform mu(*mat);
    m_prgm->update_uniform_block("MatUniformBlock", 0, sizeof(CKGLMaterialUniform), &mu);
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetViewport(CKViewportData *data)
{
    ZoneScopedN(__FUNCTION__);
    glViewport(data->ViewX, data->ViewY, data->ViewWidth, data->ViewHeight);
    glDepthRangef(data->ViewZMin, data->ViewZMax);
    m_2dvpmtx = VxMatrix::Identity();
    float (*m)[4] = (float(*)[4])&m_2dvpmtx;
    m[0][0] = 2. / data->ViewWidth;
    m[1][1] = 2. / data->ViewHeight;
    m[2][2] = 0;
    m[3][0] = -(-2. * data->ViewX + data->ViewWidth) / data->ViewWidth;
    m[3][1] =  (-2. * data->ViewY + data->ViewHeight) / data->ViewHeight;
    m_prgm->stage_uniform("mvp2d", CKGLUniformValue::make_f32mat4(1, (float*)&m_2dvpmtx));
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    ZoneScopedN(__FUNCTION__);
    CKDWORD UnityMatrixMask = 0;
    switch (Type)
    {
        case VXMATRIX_WORLD: UnityMatrixMask = WORLD_TRANSFORM; break;
        case VXMATRIX_VIEW: UnityMatrixMask = VIEW_TRANSFORM; break;
        case VXMATRIX_PROJECTION: UnityMatrixMask = PROJ_TRANSFORM; break;
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
    }
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= UnityMatrixMask;
    } else
        m_UnityMatrixMask &= ~UnityMatrixMask;

    switch (Type)
    {
        case VXMATRIX_WORLD:
        {
            m_WorldMatrix = Mat;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_prgm->stage_uniform("world", CKGLUniformValue::make_f32mat4(1, (float*)&m_WorldMatrix));
            VxMatrix tmat;
            Vx3DTransposeMatrix(tmat, Mat); //row-major to column-major conversion madness
            m_tiworldmtx = inv(tmat);
            m_prgm->stage_uniform("tiworld", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldmtx));
            Vx3DTransposeMatrix(tmat, m_ModelViewMatrix);
            m_tiworldviewmtx = inv(tmat);
            m_prgm->stage_uniform("tiworldview", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldviewmtx));
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            break;
        }
        case VXMATRIX_VIEW:
        {
            m_ViewMatrix = Mat;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_prgm->stage_uniform("view", CKGLUniformValue::make_f32mat4(1, (float*)&m_ViewMatrix));
            m_MatrixUptodate = 0;
            VxMatrix tmat;
            Vx3DInverseMatrix(tmat, Mat);
            m_viewpos = VxVector(tmat[3][0], tmat[3][1], tmat[3][2]);
            m_prgm->stage_uniform("vpos", CKGLUniformValue::make_f32v3v(1, (float*)&m_viewpos));
            Vx3DTransposeMatrix(tmat, m_ModelViewMatrix);
            m_tiworldviewmtx = inv(tmat);
            m_prgm->stage_uniform("tiworldview", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldviewmtx));
            break;
        }
        case VXMATRIX_PROJECTION:
        {
            m_ProjectionMatrix = Mat;
            m_prgm->stage_uniform("proj", CKGLUniformValue::make_f32mat4(1, (float*)&m_ProjectionMatrix));
            float (*m)[4] = (float(*)[4])&Mat;
            float A = m[2][2];
            float B = m[3][2];
            float zp[2] = {-B / A, B / (1 - A)}; //for eye-distance fog calculation
            m_prgm->stage_uniform("depth_range", CKGLUniformValue::make_f32v2v(1, zp, true));
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
        {
            CKDWORD tex = Type - VXMATRIX_TEXTURE0;
            m_textrmtx[tex] = Mat;
            m_prgm->stage_uniform("textr[" + std::to_string(tex) + "]", CKGLUniformValue::make_f32mat4(1, (float*)&m_textrmtx[tex]));
            break;
        }
        default:
            return FALSE;
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    ZoneScopedN(__FUNCTION__);
#if LOG_RENDERSTATE
    fprintf(stderr, "set render state %s -> %x, currently %x\n", rstytostr(State), Value, m_renderst[State]);
#endif
    if (m_renderst[State] != Value)
    {
        ++m_RenderStateCacheMiss;
        m_renderst[State] = Value;
        m_StateCache[State].Valid = 1;
        m_StateCache[State].Value = Value;
        return _SetRenderState(State, Value);
    } else ++m_RenderStateCacheHit;
    return TRUE;
}

CKBOOL CKGLRasterizerContext::_SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    static GLenum previous_zfunc = GL_INVALID_ENUM;
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
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    };
    auto send_light_switches = [this]() {
        m_prgm->stage_uniform("lighting_switches", CKGLUniformValue::make_u32v(1, &m_lighting_flags));
    };
    auto toggle_flag = [](uint32_t *flags, uint32_t flag, bool enabled) {
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
                    glDepthFunc(previous_zfunc);
            }
            else
                glDepthFunc(GL_ALWAYS);
            update_depth_switches();
            return TRUE;
        case VXRENDERSTATE_ZWRITEENABLE:
            glDepthMask(Value);
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
            glDepthFunc(previous_zfunc);
            return TRUE;
        case VXRENDERSTATE_ALPHABLENDENABLE:
            if (Value)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
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
            glBlendEquation(beq);
            return TRUE;
        }
        case VXRENDERSTATE_SRCBLEND:
        {
            m_blend_src = vxblend2glblfactor((VXBLEND_MODE)Value);
            if (m_use_post_processing && m_target_mode == -1)
                glBlendFuncSeparate(m_blend_src, m_blend_dst, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
            else
                glBlendFunc(m_blend_src, m_blend_dst);
            return TRUE;
        }
        case VXRENDERSTATE_DESTBLEND:
        {
            m_blend_dst = vxblend2glblfactor((VXBLEND_MODE)Value);
            if (m_use_post_processing && m_target_mode == -1)
                glBlendFuncSeparate(m_blend_src, m_blend_dst, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
            else
                glBlendFunc(m_blend_src, m_blend_dst);
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
                    glDisable(GL_CULL_FACE);
                    break;
                case VXCULL_CCW:
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CW);
                    glCullFace(m_InverseWinding ? GL_FRONT : GL_BACK);
                    break;
                case VXCULL_CW:
                    glEnable(GL_CULL_FACE);
                    glFrontFace(GL_CCW);
                    glCullFace(m_InverseWinding ? GL_FRONT : GL_BACK);
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
        case VXRENDERSTATE_ALPHAFUNC:
        {
            m_alpha_test_flags = (m_alpha_test_flags & 0x80) | Value;
            m_prgm->stage_uniform("alphatest_flags", CKGLUniformValue::make_u32v(1, &m_alpha_test_flags));
            return TRUE;
        }
        case VXRENDERSTATE_ALPHATESTENABLE:
        {
            toggle_flag(&m_alpha_test_flags, 0x80, Value);
            m_prgm->stage_uniform("alphatest_flags", CKGLUniformValue::make_u32v(1, &m_alpha_test_flags));
            return TRUE;
        }
        case VXRENDERSTATE_ALPHAREF:
        {
            m_prgm->stage_uniform("alpha_thresh", CKGLUniformValue::make_f32(Value / 255.));
            return TRUE;
        }
        case VXRENDERSTATE_FOGENABLE:
        {
            toggle_flag(&m_fog_flags, 0x80, Value);
            m_prgm->stage_uniform("fog_flags", CKGLUniformValue::make_u32v(1, &m_fog_flags));
            return TRUE;
        }
        case VXRENDERSTATE_FOGCOLOR:
        {
            VxColor col(Value);
            m_prgm->stage_uniform("fog_color", CKGLUniformValue::make_f32v4v(1, (float*)&col.col, true));
            return TRUE;
        }
        case VXRENDERSTATE_FOGPIXELMODE:
        {
            m_fog_flags = (m_fog_flags & 0x80) | Value;
            m_prgm->stage_uniform("fog_flags", CKGLUniformValue::make_u32v(1, &m_fog_flags));
            return TRUE;
        }
        case VXRENDERSTATE_FOGSTART:
        {
            m_fog_parameters[0] = reinterpret_cast<float&>(Value);
            m_prgm->stage_uniform("fog_parameters", CKGLUniformValue::make_f32v3v(1, (float*)&m_fog_parameters));
            return TRUE;
        }
        case VXRENDERSTATE_FOGEND:
        {
            m_fog_parameters[1] = reinterpret_cast<float&>(Value);
            m_prgm->stage_uniform("fog_parameters", CKGLUniformValue::make_f32v3v(1, (float*)&m_fog_parameters));
            return TRUE;
        }
        case VXRENDERSTATE_FOGDENSITY:
        {
            m_fog_parameters[2] = reinterpret_cast<float&>(Value);
            m_prgm->stage_uniform("fog_parameters", CKGLUniformValue::make_f32v3v(1, (float*)&m_fog_parameters));
            return TRUE;
        }
        case VXRENDERSTATE_SHADEMODE:
        {
            //not supported. we are stuck in Phong forever.
            return FALSE;
        }
        case VXRENDERSTATE_CLIPPING:
        {
            //not supported. always on.
            return FALSE;
        }
        case VXRENDERSTATE_DITHERENABLE:
        {
            //not supported. always on.
            return FALSE;
        }
        case VXRENDERSTATE_TEXTUREPERSPECTIVE:
        {
            //not supported. always on.
            return FALSE;
        }
        case VXRENDERSTATE_NORMALIZENORMALS:
        {
            //not supported. always on.
            return FALSE;
        }
        case VXRENDERSTATE_AMBIENT:
        {
            //needs changes in the current fragment shader.
            return FALSE;
        }
        case VXRENDERSTATE_FILLMODE:
        {
            switch (Value)
            {
                case VXFILL_POINT: glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); break;
                case VXFILL_WIREFRAME: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); break;
                case VXFILL_SOLID: glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); break;
            }
            return TRUE;
        }
        default:
            fprintf(stderr, "unhandled render state %s -> %d\n", rstytostr(State), Value);
            return FALSE;
    }
}

void CKGLRasterizerContext::toggle_console(int t)
{
    static bool enabled = false;
    if (t == 0) enabled ^= true;
    else enabled = t > 0;
    if (enabled)
    {
        AllocConsole();
        freopen("CON", "r", stdin);
        freopen("CON", "w", stdout);
        freopen("CON", "w", stderr);
    }
    else
    {
        FreeConsole();
        freopen("NUL", "r", stdin);
        freopen("NUL", "w", stdout);
        freopen("NUL", "w", stderr);
    }
}

void CKGLRasterizerContext::set_step_mode(int mode)
{
    if (mode > 0) toggle_console(1);
    else set_title_status(nullptr);
    m_step_mode = mode;
}

void CKGLRasterizerContext::step_mode_wait()
{
    while (true)
    {
        HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
        WaitForSingleObject(hstdin, INFINITE);
        INPUT_RECORD rcd;
        DWORD nevt, nread;
        if (!GetNumberOfConsoleInputEvents(hstdin, &nevt)) return;
        bool ret = false;
        for (DWORD i = 0; i < nevt; ++i)
        {
            if (!ReadConsoleInputA(hstdin, &rcd, 1, &nread)) return;
            if (rcd.EventType == KEY_EVENT && rcd.Event.KeyEvent.bKeyDown)
            {
                if (rcd.Event.KeyEvent.wVirtualKeyCode == 'X')
                    set_step_mode(0);
                ret = true;
            }
        }
        if (ret) break;
    }
    fputs("\n", stderr);
}

void CKGLRasterizerContext::toggle_batch_status()
{
    if (!(m_batch_status ^= 1))
        set_title_status(nullptr);
}

void CKGLRasterizerContext::toggle_specular_handling()
{
    CKDWORD next = 0;
    if (m_lighting_flags & LSW_SPCL_OVERR_FORCE)
        next = LSW_SPCL_OVERR_ONLY;
    else if (m_lighting_flags & LSW_SPCL_OVERR_ONLY)
        next = 0;
    else next = LSW_SPCL_OVERR_FORCE;
    m_lighting_flags &= ~0U ^ (LSW_SPCL_OVERR_FORCE | LSW_SPCL_OVERR_ONLY);
    m_lighting_flags |= next;
    m_prgm->stage_uniform("lighting_switches", CKGLUniformValue::make_u32v(1, &m_lighting_flags));
}

void CKGLRasterizerContext::toggle_2d_rendering()
{
    if (m_use_post_processing)
        m_2d_enabled = !m_2d_enabled;
}

void CKGLRasterizerContext::cycle_post_processing_shader()
{
    if (m_use_post_processing)
        m_ppsh_switch_pending = true;
}

void CKGLRasterizerContext::toggle_post_processing() { m_use_pp_switch_pending = true; }

CKBOOL CKGLRasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}

CKBOOL CKGLRasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
#if LOG_SETTEXTURE
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    if ((Texture == 0) ^ ((m_null_texture_mask >> Stage) & 1))
    {
        m_null_texture_mask &= ~(1 << Stage);
        m_null_texture_mask |= ((Texture == 0) << Stage);
        m_prgm->stage_uniform("null_texture_mask", CKGLUniformValue::make_u32v(1, &m_null_texture_mask));
    }
    if (Stage != m_cur_ts)
    {
        glActiveTexture(GL_TEXTURE0 + Stage);
        m_cur_ts = Stage;
    }
    m_ts_texture[Stage] = Texture;
    CKGLTexture *desc = static_cast<CKGLTexture *>(m_Textures[Texture]);
    if (!desc)
        glBindTexture(GL_TEXTURE_2D, 0);
    else
        desc->Bind();
    return TRUE;
}

CKBOOL CKGLRasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    ZoneScopedN(__FUNCTION__);
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
    auto vxfiltermode2glfilter = [](VXTEXTURE_FILTERMODE fm) -> GLenum
    {
        switch (fm)
        {
            case VXTEXTUREFILTER_NEAREST: return GL_NEAREST;
            case VXTEXTUREFILTER_LINEAR: return GL_LINEAR;
            //we don't generate mipmap yet
            case VXTEXTUREFILTER_MIPNEAREST: return GL_NEAREST_MIPMAP_NEAREST;
            case VXTEXTUREFILTER_MIPLINEAR: return GL_NEAREST_MIPMAP_LINEAR;
            case VXTEXTUREFILTER_LINEARMIPNEAREST: return GL_LINEAR_MIPMAP_NEAREST;
            case VXTEXTUREFILTER_LINEARMIPLINEAR: return GL_LINEAR_MIPMAP_LINEAR;
            //needs ARB_texture_filter_anisotropic or EXT_texture_filter_anisotropic...
            case VXTEXTUREFILTER_ANISOTROPIC: return GL_LINEAR;
            default: return GL_INVALID_ENUM;
        }
    };
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                int glwrapval = vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value);
                t->set_parameter(GL_TEXTURE_WRAP_S, glwrapval);
                t->set_parameter(GL_TEXTURE_WRAP_T, glwrapval);
                t->set_parameter(GL_TEXTURE_WRAP_R, glwrapval);
            }
            return TRUE;
        case CKRST_TSS_ADDRESSU:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                int glwrapval = vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value);
                t->set_parameter(GL_TEXTURE_WRAP_S, glwrapval);
            }
            return TRUE;
        case CKRST_TSS_ADDRESSV:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                int glwrapval = vxaddrmode2glwrap((VXTEXTURE_ADDRESSMODE)Value);
                t->set_parameter(GL_TEXTURE_WRAP_T, glwrapval);
            }
            return TRUE;
        case CKRST_TSS_BORDERCOLOR:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                t->set_border_color((int)Value);
            }
            return TRUE;
        case CKRST_TSS_MINFILTER:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                int glfilterval = vxfiltermode2glfilter((VXTEXTURE_FILTERMODE)Value);
                t->set_parameter(GL_TEXTURE_MIN_FILTER, glfilterval);
            }
            return TRUE;
        case CKRST_TSS_MAGFILTER:
            if (~m_ts_texture[Stage])
            {
                CKGLTexture *t = static_cast<CKGLTexture*>(m_Textures[m_ts_texture[Stage]]);
                int glfilterval = vxfiltermode2glfilter((VXTEXTURE_FILTERMODE)Value);
                t->set_parameter(GL_TEXTURE_MAG_FILTER, glfilterval);
            }
            return TRUE;
        case CKRST_TSS_STAGEBLEND:
        {
            CKGLTexCombinatorUniform tc = m_texcombo[Stage];
            bool valid = true;
            switch (Value)
            {
                case STAGEBLEND(VXBLEND_ZERO, VXBLEND_SRCCOLOR):
                case STAGEBLEND(VXBLEND_DESTCOLOR, VXBLEND_ZERO):
                    tc = CKGLTexCombinatorUniform::make(
                        TexOp::modulate, TexArg::texture, TexArg::current, TexArg::current,
                        TexOp::select1,  TexArg::current, TexArg::current, TexArg::current,
                        tc.dest(), tc.constant);
                    break;
                case STAGEBLEND(VXBLEND_ONE, VXBLEND_ONE):
                    tc = CKGLTexCombinatorUniform::make(
                        TexOp::add, TexArg::current, TexArg::current, TexArg::current,
                        TexOp::select1,  TexArg::current, TexArg::current, TexArg::current,
                        tc.dest(), tc.constant);
                    break;
                default:
                    valid = false;
            }
            if (valid)
            {
                m_texcombo[Stage] = tc;
                //m_prgm->update_uniform_block("TexCombinatorUniformBlock", Stage * sizeof(CKGLTexCombinatorUniform), sizeof(CKGLTexCombinatorUniform), &m_texcombo[Stage]);
                m_prgm->update_uniform_block("TexCombinatorUniformBlock", 0, CKRST_MAX_STAGES * sizeof(CKGLTexCombinatorUniform), m_texcombo);
            }
            return valid;
        }
        case CKRST_TSS_TEXTUREMAPBLEND:
        {
            CKGLTexCombinatorUniform tc = m_texcombo[Stage];
            bool valid = true;
            switch (Value)
            {
                case VXTEXTUREBLEND_DECAL:
                case VXTEXTUREBLEND_COPY:
                    tc.set_color_op(TexOp::select1);
                    tc.set_color_arg1(TexArg::texture);
                    tc.set_alpha_op(TexOp::select1);
                    tc.set_alpha_arg1(TexArg::texture);
                    break;
                case VXTEXTUREBLEND_MODULATE:
                case VXTEXTUREBLEND_MODULATEALPHA:
                case VXTEXTUREBLEND_MODULATEMASK:
                    tc = CKGLTexCombinatorUniform::make(
                        TexOp::modulate, TexArg::texture, TexArg::current, TexArg::current,
                        TexOp::modulate, TexArg::texture, TexArg::current, TexArg::current,
                        tc.dest(), tc.constant);
                    break;
                case VXTEXTUREBLEND_DECALALPHA:
                case VXTEXTUREBLEND_DECALMASK:
                    tc.set_color_op(TexOp::mixtexalp);
                    tc.set_color_arg1(TexArg::texture);
                    tc.set_alpha_arg2(TexArg::current);
                    tc.set_alpha_op(TexOp::select1);
                    tc.set_alpha_arg1(TexArg::diffuse);
                    break;
                case VXTEXTUREBLEND_ADD:
                    tc.set_color_op(TexOp::add);
                    tc.set_color_arg1(TexArg::texture);
                    tc.set_alpha_arg2(TexArg::current);
                    tc.set_alpha_op(TexOp::select1);
                    tc.set_alpha_arg1(TexArg::current);
                    break;
                default:
                    valid = false;
            }
            if (valid)
            {
                m_texcombo[Stage] = tc;
                //m_prgm->update_uniform_block("TexCombinatorUniformBlock", Stage * sizeof(CKGLTexCombinatorUniform), sizeof(CKGLTexCombinatorUniform), &m_texcombo[Stage]);
                m_prgm->update_uniform_block("TexCombinatorUniformBlock", 0, CKRST_MAX_STAGES * sizeof(CKGLTexCombinatorUniform), m_texcombo);
            }
            return valid;
        }
        case CKRST_TSS_TEXTURETRANSFORMFLAGS:
        {
            CKDWORD tvp = m_tex_vp[Stage];
            if (!Value)
                tvp &= ~0U ^ TVP_TC_TRANSF;
            else tvp |= TVP_TC_TRANSF;
            if (Value & CKRST_TTF_PROJECTED)
                tvp |= TVP_TC_PROJECTED;
            else tvp &= ~0U ^ TVP_TC_PROJECTED;
            if (tvp != m_tex_vp[Stage])
            {
                m_tex_vp[Stage] = tvp;
                m_prgm->stage_uniform("texp[" + std::to_string(Stage) + "]", CKGLUniformValue::make_u32v(1, (uint32_t*)&m_tex_vp[Stage]));
            }
            return TRUE;
        }
        case CKRST_TSS_TEXCOORDINDEX:
        {
            //we currently ignore the texture coords index encoded in Value...
            //because we simply don't have that in our frag shader...
            //we only care about automatic texture coords generation for now.
            CKDWORD tvp = m_tex_vp[Stage] & (~0U ^ 0x07000000U);
            switch (Value >> 16)
            {
                case 0: break;
                case 1: tvp |= TVP_TC_CSNORM; break;
                case 2: tvp |= TVP_TC_CSVECP; break;
                case 3: tvp |= TVP_TC_CSREFV; break;
                default: return FALSE;
            }
            if (tvp != m_tex_vp[Stage])
            {
                m_tex_vp[Stage] = tvp;
                m_prgm->stage_uniform("texp[" + std::to_string(Stage) + "]", CKGLUniformValue::make_u32v(1, (uint32_t*)&m_tex_vp[Stage]));
            }
            return TRUE;
        }
        default:
            fprintf(stderr, "unhandled texture stage state %s -> %d\n", tstytostr(Tss), Value);
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
#if LOG_DRAWPRIMITIVE
    fprintf(stderr, "drawprimitive ib %p %d\n", indices, indexcount);
#endif
    ++directbat;
    ZoneScopedN(__FUNCTION__);
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
    CKGLVertexBuffer *vbo = nullptr;
    auto vboid = std::make_pair(vertexFormat, DWORD(m_direct_draw_counter));
    if (++ m_direct_draw_counter > DYNAMIC_VBO_COUNT) m_direct_draw_counter = 0;
    if (m_dynvbo.find(vboid) == m_dynvbo.end() ||
        m_dynvbo[vboid]->m_MaxVertexCount < data->VertexCount)
    {
        if (m_dynvbo[vboid])
            delete m_dynvbo[vboid];
        CKVertexBufferDesc vbd;
        vbd.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        vbd.m_VertexFormat = vertexFormat;
        vbd.m_VertexSize = vertexSize;
        vbd.m_MaxVertexCount = (data->VertexCount + 100 > DEFAULT_VB_SIZE) ? data->VertexCount + 100 : DEFAULT_VB_SIZE;
        CKGLVertexBuffer *vb = new CKGLVertexBuffer(&vbd);
        vb->Create();
        m_dynvbo[vboid] = vb;
    }
    vbo = m_dynvbo[vboid];
    vbo->Bind(this);
    void *pbData = nullptr;
    CKDWORD vbase = 0;
    if (vbo->m_CurrentVCount + data->VertexCount <= vbo->m_MaxVertexCount)
    {
        TracyPlot("Lock offset", (int64_t)vertexSize * vbo->m_CurrentVCount);
        TracyPlot("Lock len", (int64_t)vertexSize * data->VertexCount);
        pbData = vbo->Lock(vertexSize * vbo->m_CurrentVCount,
                           vertexSize * data->VertexCount, false);
        vbase = vbo->m_CurrentVCount;
        vbo->m_CurrentVCount += data->VertexCount;
    } else
    {
        TracyPlot("Lock offset", 0ll);
        TracyPlot("Lock len", (int64_t)vertexSize * data->VertexCount);
        pbData = vbo->Lock(0, vertexSize * data->VertexCount, true);
        vbo->m_CurrentVCount = data->VertexCount;
    }
    {
        ZoneScopedN("CKRSTLoadVertexBuffer");
        CKRSTLoadVertexBuffer(static_cast<CKBYTE *>(pbData), vertexFormat, vertexSize, data);
    }
    vbo->Unlock();
    return InternalDrawPrimitive(pType, vbo, vbase, data->VertexCount, indices, indexcount, true);
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
    CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if LOG_DRAWPRIMITIVEVB
    fprintf(stderr, "drawprimitive vb %d %d\n", VertexCount, indexcount);
#endif
    ++vbbat;
    ZoneScopedN(__FUNCTION__);
    return InternalDrawPrimitive(VXPRIMITIVETYPE((CKDWORD)pType | 0x100), static_cast<CKGLVertexBuffer*>(m_VertexBuffers[VertexBuffer]),
        StartIndex, VertexCount, indices, indexcount);
}

CKBOOL CKGLRasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
    CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if LOG_DRAWPRIMITIVEVBIB
    fprintf(stderr, "drawprimitive vbib %d %d\n", VertexCount, Indexcount);
#endif
    ++vbibbat;
    ZoneScopedN(__FUNCTION__);

    if (VB >= m_VertexBuffers.Size()) return NULL;
    CKGLVertexBuffer *vbo = static_cast<CKGLVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vbo) return NULL;

    if (IB >= m_IndexBuffers.Size()) return NULL;
    CKGLIndexBuffer *ibo = static_cast<CKGLIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ibo) return NULL;

    if (m_use_post_processing)
        select_framebuffer(vbo->m_VertexFormat & CKRST_VF_RASTERPOS);

    vbo->Bind(this);
#if USE_SEPARATE_ATTRIBUTE
    if (m_current_vf != vbo->m_VertexFormat)
    {
        get_vertex_format((CKRST_VERTEXFORMAT)vbo->m_VertexFormat)->select(this);
        m_current_vf = vbo->m_VertexFormat;
    }
    vbo->bind_to_array();
#endif
    ibo->Bind();

    m_prgm->send_uniform();

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
    glDrawElementsBaseVertex(glpt, Indexcount, GL_UNSIGNED_SHORT, (void*)(2 * StartIndex), MinVIndex);
    if (m_step_mode == 2)
    {
        BackToFront(FALSE);
        glDrawElementsBaseVertex(glpt, Indexcount, GL_UNSIGNED_SHORT, (void*)(2 * StartIndex), MinVIndex);
        BackToFront(FALSE);
        set_title_status("stepping - DrawPrimitiveVBIB | X in console = quit step mode | Any key = next");
        step_mode_wait();
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKGLVertexBuffer *vbo, CKDWORD vbase, CKDWORD vcnt, WORD* idx, GLuint icnt, bool vbbound)
{
    ZoneScopedN(__FUNCTION__);

    if (m_use_post_processing)
        select_framebuffer(vbo->m_VertexFormat & CKRST_VF_RASTERPOS);

    if (!vbo) return FALSE;
    if (!vbbound) vbo->Bind(this);

#if USE_SEPARATE_ATTRIBUTE
    if (m_current_vf != vbo->m_VertexFormat)
    {
        get_vertex_format((CKRST_VERTEXFORMAT)vbo->m_VertexFormat)->select(this);
        m_current_vf = vbo->m_VertexFormat;
    }
    vbo->bind_to_array();
#endif

    int ibbase = 0;
    if (idx)
    {
        void *pdata = nullptr;
        auto iboid = m_noibo_draw_counter;
        if (++ m_noibo_draw_counter > DYNAMIC_IBO_COUNT) m_noibo_draw_counter = 0;
        if (m_dynibo.find(iboid) == m_dynibo.end() ||
            m_dynibo[iboid]->m_MaxIndexCount < icnt)
        {
            if (m_dynibo[iboid])
                delete m_dynibo[iboid];
            CKIndexBufferDesc ibd;
            ibd.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
            ibd.m_MaxIndexCount = icnt + 100 < DEFAULT_VB_SIZE ? DEFAULT_VB_SIZE : icnt + 100;
            ibd.m_CurrentICount = 0;
            CKGLIndexBuffer *ib = new CKGLIndexBuffer(&ibd);
            ib->Create();
            m_dynibo[iboid] = ib;
        }
        CKGLIndexBuffer *ibo = m_dynibo[iboid];
        ibo->Bind();
        if (icnt + ibo->m_CurrentICount <= ibo->m_MaxIndexCount)
        {
            pdata = ibo->Lock(2 * ibo->m_CurrentICount, 2 * icnt, false);
            ibbase = 2 * ibo->m_CurrentICount;
            ibo->m_CurrentICount += icnt;
        } else
        {
            pdata = ibo->Lock(0, 2 * icnt, true);
            ibo->m_CurrentICount = icnt;
        }
        if (pdata)
            memcpy(pdata, idx, 2 * icnt);
        ibo->Unlock();
    }

    m_prgm->send_uniform();

    GLenum glpt = GL_NONE;
    switch (pType & 0xf)
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
        glDrawElementsBaseVertex(glpt, icnt, GL_UNSIGNED_SHORT, (void*)ibbase, vbase);
    }
    else
        glDrawArrays(glpt, vbase, vcnt);
    if (m_step_mode == 2)
    {
        BackToFront(FALSE);
        if (idx)
        {
            glDrawElementsBaseVertex(glpt, icnt, GL_UNSIGNED_SHORT, (void*)ibbase, vbase);
        }
        else
            glDrawArrays(glpt, vbase, vcnt);
        BackToFront(FALSE);
        set_title_status("stepping - %s | X in console = quit step mode | Any key = next",
            pType & 0x100 ? "DrawPrimitiveVB" : "DrawPrimitive");
        step_mode_wait();
    }
    return TRUE;
}

CKBOOL CKGLRasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    int result;

    if (ObjIndex >= m_Textures.Size())
        return 0;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
            result = CreateSpriteNPOT(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
            break;
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
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size()) return NULL;
    CKGLVertexBuffer *vbo = static_cast<CKGLVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vbo) return NULL;
    return vbo->Lock(StartVertex * vbo->m_VertexSize, VertexCount * vbo->m_VertexSize, (Lock & CKRST_LOCK_NOOVERWRITE) == 0);
}

CKBOOL CKGLRasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size()) return FALSE;
    CKGLVertexBuffer *vbo = static_cast<CKGLVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vbo) return FALSE;
    vbo->Unlock();
    return TRUE;
}

CKBOOL CKGLRasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKGLTexture *desc = static_cast<CKGLTexture *>(m_Textures[Texture]);
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
    desc->Bind();
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

CKBOOL CKGLRasterizerContext::LoadSprite(CKDWORD Sprite, const VxImageDescEx &SurfDesc)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    LoadTexture(spr->Textures.Front().IndexTexture, SurfDesc);
    return TRUE;
}

CKBOOL CKGLRasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    VxDrawPrimitiveData pd{};
    pd.VertexCount = 4;
    pd.Flags = CKRST_DP_VCT;
    VxVector4 p[4] = {
        VxVector4(dst->left , dst->top   , 0, 1.),
        VxVector4(dst->right, dst->top   , 0, 1.),
        VxVector4(dst->right, dst->bottom, 0, 1.),
        VxVector4(dst->left , dst->bottom, 0, 1.)
    };
    CKDWORD c[4] = {~0U, ~0U, ~0U, ~0U};
    Vx2DVector t[4] = {
        Vx2DVector(src->left , src->top),
        Vx2DVector(src->right, src->top),
        Vx2DVector(src->right, src->bottom),
        Vx2DVector(src->left , src->bottom)
    };
    for (int i = 0; i < 4; ++i)
    {
        t[i].x /= spr->Format.Width;
        t[i].y /= spr->Format.Height;
    }
    pd.PositionStride = sizeof(VxVector4);
    pd.ColorStride = sizeof(CKDWORD);
    pd.TexCoordStride = sizeof(Vx2DVector);
    pd.PositionPtr = p;
    pd.ColorPtr = c;
    pd.TexCoordPtr = t;
    CKWORD idx[6] = {0, 1, 2, 0, 2, 3};
    SetTexture(spr->Textures.Front().IndexTexture);
    _SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    _SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    DrawPrimitive(VX_TRIANGLELIST, idx, 6, &pd);
    _SetRenderState(VXRENDERSTATE_CULLMODE, m_renderst[VXRENDERSTATE_CULLMODE]);
    _SetRenderState(VXRENDERSTATE_LIGHTING, m_renderst[VXRENDERSTATE_LIGHTING]);
    return TRUE;
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
    if (IB >= m_IndexBuffers.Size()) return NULL;
    CKGLIndexBuffer *ibo = static_cast<CKGLIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ibo) return NULL;
    return ibo->Lock(StartIndex * 2, IndexCount * 2, (Lock & CKRST_LOCK_NOOVERWRITE) == 0);
}

CKBOOL CKGLRasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB >= m_IndexBuffers.Size()) return NULL;
    CKGLIndexBuffer *ibo = static_cast<CKGLIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ibo) return NULL;
    ibo->Unlock();
    return TRUE;
}

CKGLVertexFormat *CKGLRasterizerContext::get_vertex_format(CKRST_VERTEXFORMAT vf)
{
    if (m_vertfmts.find(vf) == m_vertfmts.end())
        m_vertfmts[vf] = new CKGLVertexFormat(vf);
    return m_vertfmts[vf];
}

unsigned CKGLRasterizerContext::get_vertex_attrib_location(CKDWORD component)
{
    const static std::unordered_map<CKDWORD, unsigned> loc = {
        {CKRST_VF_POSITION, 0},
        {CKRST_VF_RASTERPOS, 0},
        {CKRST_VF_NORMAL, 1},
        {CKRST_VF_DIFFUSE, 2},
        {CKRST_VF_SPECULAR, 3},
        {CKRST_VF_TEX1, 4},
        {CKRST_VF_TEX2, 5}
    };
    return loc.find(component) != loc.end() ? loc.find(component)->second : ~0;
}

void CKGLRasterizerContext::set_position_transformed(bool transformed)
{
    if (bool(m_cur_vp & VP_IS_TRANSFORMED) ^ transformed)
    {
        m_cur_vp &= ~0U ^ VP_IS_TRANSFORMED;
        if (transformed) m_cur_vp |= VP_IS_TRANSFORMED;
        m_prgm->stage_uniform("vertex_properties", CKGLUniformValue::make_u32v(1, &m_cur_vp));
    }
}

void CKGLRasterizerContext::set_vertex_has_color(bool color)
{
    if (bool(m_cur_vp & VP_HAS_COLOR) ^ color)
    {
        m_cur_vp &= ~0U ^ VP_HAS_COLOR;
        if (color) m_cur_vp |= VP_HAS_COLOR;
        m_prgm->stage_uniform("vertex_properties", CKGLUniformValue::make_u32v(1, &m_cur_vp));
    }
}

void CKGLRasterizerContext::set_num_textures(CKDWORD ntex)
{
    if (ntex <= 8 && (m_cur_vp & VP_TEXTURE_MASK) != ntex)
    {
        m_cur_vp &= ~VP_TEXTURE_MASK;
        m_cur_vp |= ntex;
        m_prgm->stage_uniform("vertex_properties", CKGLUniformValue::make_u32v(1, &m_cur_vp));
    }
}

void CKGLRasterizerContext::select_framebuffer(bool twod)
{
    if (twod)
    {
        if (m_target_mode != -1) //needs pre-multiplied alpha
        {
            m_target_mode = -1;
            m_2dpp->set_as_target();
            glBlendFuncSeparate(m_blend_src, m_blend_dst, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        }
    }
    else
    {
        if (m_target_mode != 1)
        {
            m_target_mode = 1;
            m_3dpp->set_as_target();
            glBlendFunc(m_blend_src, m_blend_dst);
        }
    }
}

void CKGLRasterizerContext::set_title_status(const char *fmt, ...)
{
    std::string ts;
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        va_list argsx;
        va_copy(argsx, args);
        ts.resize(vsnprintf(NULL, 0, fmt, argsx) + 1);
        va_end(argsx);
        vsnprintf(ts.data(), ts.size(), fmt, args);
        va_end(args);
        ts = m_orig_title + " | " + ts;
    } else ts = m_orig_title;

    SetWindowTextA(GetAncestor((HWND)m_Window, GA_ROOT), ts.c_str());
}

CKBOOL CKGLRasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    if (m_Textures[Texture])
        return TRUE;
#if LOG_CREATETEXTURE
    fprintf(stderr, "create texture %d %dx%d %x\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height, DesiredFormat->Flags);
#endif

    CKGLTexture *desc = new CKGLTexture(DesiredFormat);
    m_Textures[Texture] = desc;
    desc->Create();
    return TRUE;
}

CKBOOL CKGLRasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKGLRasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKGLRasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return 0;
    delete m_VertexBuffers[VB];

    CKGLVertexBuffer* desc = new CKGLVertexBuffer(DesiredFormat);
    desc->Create();
    m_VertexBuffers[VB] = desc;
#if LOG_CREATEBUFFER
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
    ZoneScopedN(__FUNCTION__);
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return 0;
    delete m_IndexBuffers[IB];
    CKGLIndexBuffer* desc = new CKGLIndexBuffer(DesiredFormat);
    desc->Create();
    m_IndexBuffers[IB] = desc;
    return 1;
}

CKBOOL CKGLRasterizerContext::CreateSpriteNPOT(CKDWORD Sprite, CKSpriteDesc *DesiredFormat)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size() || !DesiredFormat)
        return FALSE;
    if (m_Sprites[Sprite])
        delete m_Sprites[Sprite];
    m_Sprites[Sprite] = new CKSpriteDesc();
    CKSpriteDesc *spr = m_Sprites[Sprite];
    spr->Flags = DesiredFormat->Flags;
    spr->Format = DesiredFormat->Format;
    spr->MipMapCount = DesiredFormat->MipMapCount;
    spr->Owner = m_Driver->m_Owner;
    CKSPRTextInfo ti;
    ti.IndexTexture = m_Driver->m_Owner->CreateObjectIndex(CKRST_OBJ_TEXTURE);
    CreateObject(ti.IndexTexture, CKRST_OBJ_TEXTURE, DesiredFormat);
    spr->Textures.PushBack(ti);
    return TRUE;
}

VxMatrix inv(const VxMatrix &_m)
{
    ZoneScopedN(__FUNCTION__);
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