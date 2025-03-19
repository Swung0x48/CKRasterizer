#include "CKDX9Rasterizer.h"

#include "CKD3DX9.h"

#define LOGGING 0
#define STEP 0
#define LOG_LOADTEXTURE 0
#define LOG_CREATETEXTURE 0
#define LOG_DRAWPRIMITIVE 0
#define LOG_SETTEXURESTAGESTATE 0
#define LOG_FLUSHCACHES 0
#define LOG_BATCHSTATS 0

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

CKBOOL CKDX9VertexShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKVertexShaderDesc *Format)
{
    if (Format != this)
    {
        Owner = Ctx;
        m_Function = Format->m_Function;
        m_FunctionSize = Format->m_FunctionSize;
    }

    if (!m_Function || m_FunctionSize == 0 || !Ctx || !Ctx->m_Device)
        return FALSE;

    SAFERELEASE(DxShader);
    return SUCCEEDED(Ctx->m_Device->CreateVertexShader(m_Function, &DxShader));
}

CKDX9VertexShaderDesc::~CKDX9VertexShaderDesc() { SAFERELEASE(DxShader); }

CKBOOL CKDX9PixelShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKPixelShaderDesc *Format)
{
    if (Format != this)
    {
        Owner = Ctx;
        m_Function = Format->m_Function;
        m_FunctionSize = Format->m_FunctionSize;
    }

    if (!m_Function || m_FunctionSize == 0 || !Ctx || !Ctx->m_Device)
        return FALSE;

    SAFERELEASE(DxShader);
    return SUCCEEDED(Ctx->m_Device->CreatePixelShader(m_Function, &DxShader));
}

CKDX9PixelShaderDesc::~CKDX9PixelShaderDesc() { SAFERELEASE(DxShader); }

CKDX9RasterizerContext::CKDX9RasterizerContext(CKDX9RasterizerDriver *driver) :
    CKRasterizerContext(), m_Device(NULL), m_PresentParams(), m_DirectXData(), m_SoftwareVertexProcessing(FALSE),
    m_CurrentTextureIndex(0), m_IndexBuffer(), m_DefaultBackBuffer(NULL), m_DefaultDepthBuffer(NULL),
    m_InCreateDestroy(TRUE), m_ScreenBackup(NULL), m_CurrentVertexShaderCache(0), m_CurrentVertexFormatCache(0),
    m_CurrentVertexBufferCache(NULL), m_CurrentVertexSizeCache(0), m_TranslatedRenderStates(), m_TempZBuffers()
{
    assert(driver != NULL);
    m_Driver = driver;
    m_Owner = static_cast<CKDX9Rasterizer *>(driver->m_Owner);
}

CKDX9RasterizerContext::~CKDX9RasterizerContext()
{
    ReleaseStateBlocks();
    ReleaseIndexBuffers();
    FlushObjects(CKRST_OBJ_ALL);
    ReleaseScreenBackup();
    ReleaseVertexDeclarations();
    SAFERELEASE(m_DefaultBackBuffer);
    SAFERELEASE(m_DefaultDepthBuffer);
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;
    SAFERELEASE(m_Device);
}

int DepthBitPerPixelFromFormat(D3DFORMAT Format, CKDWORD *StencilSize)
{
    int result;
    switch (Format)
    {
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D16:
            result = 16;
            *StencilSize = 0;
            break;
        case D3DFMT_D15S1:
            *StencilSize = 1;
            result = 16;
            break;
        case D3DFMT_D24S8:
            result = 24;
            *StencilSize = 8;
            break;
        case D3DFMT_D24X8:
            result = 24;
            *StencilSize = 0;
            break;
        case D3DFMT_D32:
            *StencilSize = 0;
        default:
            result = 32;
            break;
    }
    return result;
}

CKBOOL CKDX9RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
                                      CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
#if (STEP) || (LOGGING)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
    m_InCreateDestroy = TRUE;

    // Store original window position
    CKRECT rect = {0, 0, 0, 0};
    LONG originalStyle = 0;
    if (Window)
    {
        VxGetWindowRect(Window, &rect);
        WIN_HANDLE parent = VxGetParent(Window);
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&rect));
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&rect.right));
        
        // Save original window style for restoration if needed
        originalStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
    }

    // Modify window style for fullscreen mode
    if (Fullscreen && Window)
    {
        SetWindowLongA((HWND)Window, GWL_STYLE, originalStyle & ~WS_CHILDWINDOW);
    }

    CKDX9RasterizerDriver *driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
    memset(&m_PresentParams, 0, sizeof(m_PresentParams));
    m_PresentParams.hDeviceWindow = (HWND)Window;
    m_PresentParams.BackBufferWidth = Width;
    m_PresentParams.BackBufferHeight = Height;
#ifdef ENABLE_TRIPLE_BUFFER
    m_PresentParams.BackBufferCount = 2; // Triple buffering
#else
    m_PresentParams.BackBufferCount = 1; // Double buffering
#endif
    m_PresentParams.Windowed = !Fullscreen;
#ifdef USE_D3D9EX
    m_PresentParams.SwapEffect = D3DSWAPEFFECT_FLIPEX;
#else
    m_PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
#endif
    m_PresentParams.EnableAutoDepthStencil = TRUE;
    m_PresentParams.FullScreen_RefreshRateInHz = Fullscreen ? RefreshRate : D3DPRESENT_RATE_DEFAULT;
    m_PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    // m_PresentParams.PresentationInterval = Fullscreen ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_DEFAULT;
    m_PresentParams.BackBufferFormat = driver->FindNearestRenderTargetFormat(Bpp, !Fullscreen);
    m_PresentParams.AutoDepthStencilFormat = driver->FindNearestDepthFormat(m_PresentParams.BackBufferFormat, Zbpp, StencilBpp);

    D3DDISPLAYMODEEX displayMode = {
        sizeof(D3DDISPLAYMODEEX),
        (UINT)Width,
        (UINT)Height,
        (UINT)RefreshRate,
        m_PresentParams.BackBufferFormat,
        D3DSCANLINEORDERING_PROGRESSIVE
    };

    // Configure multisampling (anti-aliasing)
    if (m_Antialias == D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
    }
    else if (m_Antialias < D3DMULTISAMPLE_2_SAMPLES || m_Antialias > D3DMULTISAMPLE_4_SAMPLES)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
    }
    else
    {
        m_PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)m_Antialias;
    }

    // Find highest supported multisampling level if requested
    if (m_PresentParams.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
    {
        for (int type = m_PresentParams.MultiSampleType; type >= D3DMULTISAMPLE_2_SAMPLES; --type)
        {
            if (SUCCEEDED(m_Owner->m_D3D9->CheckDeviceMultiSampleType(
                    static_cast<CKDX9RasterizerDriver *>(m_Driver)->m_AdapterIndex,
                    D3DDEVTYPE_HAL,
                    m_PresentParams.BackBufferFormat,
                    m_PresentParams.Windowed,
                    (D3DMULTISAMPLE_TYPE)type,
                    NULL)))
            {
                m_PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)type;
                break;
            }
        }
    }

    // Configure device behavior flags
    DWORD behaviorFlags = D3DCREATE_ENABLE_PRESENTSTATS;
    if (!driver->m_IsHTL || driver->m_D3DCaps.VertexShaderVersion < D3DVS_VERSION(1, 0) && m_EnsureVertexShader)
    {
        m_SoftwareVertexProcessing = TRUE;
        behaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
    else
    {
        behaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        if (driver->m_D3DCaps.DevCaps & D3DDEVCAPS_PUREDEVICE)
            behaviorFlags |= D3DCREATE_PUREDEVICE;
        m_SoftwareVertexProcessing = FALSE;
    }

    HRESULT hr = E_FAIL;

    // Create the Direct3D device
#ifdef USE_D3D9EX
    hr = m_Owner->m_D3D9->CreateDeviceEx(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                         behaviorFlags, &m_PresentParams, Fullscreen ? &displayMode : NULL, &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        // Retry without multisampling if it failed
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDeviceEx(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                             behaviorFlags, &m_PresentParams, Fullscreen ? &displayMode : NULL, &m_Device);
    }
#else
    hr = m_Owner->m_D3D9->CreateDevice(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow, behaviorFlags, &m_PresentParams, &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        // Retry without multisampling if it failed
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDevice(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow, behaviorFlags, &m_PresentParams, &m_Device);
    }
#endif

    // Restore window style if we're not going fullscreen
    if (Fullscreen && Window)
    {
        SetWindowLongA((HWND)Window, GWL_STYLE, originalStyle | WS_CHILDWINDOW);
    }
    else if (Window && !Fullscreen)
    {
        VxMoveWindow(Window, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
    }

    // Handle device creation failure
    if (FAILED(hr))
    {
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Store context configuration
    m_Window = (HWND)Window;
    m_PosX = PosX;
    m_PosY = PosY;
    m_Fullscreen = Fullscreen;

    // Get back buffer information
    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        VxImageDescEx desc;
        D3DSURFACE_DESC surfaceDesc;
        if (SUCCEEDED(pBackBuffer->GetDesc(&surfaceDesc)))
        {
            m_PixelFormat = D3DFormatToVxPixelFormat(surfaceDesc.Format);
            VxPixelFormat2ImageDesc(m_PixelFormat, desc);
            m_Bpp = desc.BitsPerPixel;
            m_Width = surfaceDesc.Width;
            m_Height = surfaceDesc.Height;
        }
        SAFERELEASE(pBackBuffer);
    }

    // Get depth/stencil information
    IDirect3DSurface9 *pStencilSurface = NULL;
    if (SUCCEEDED(m_Device->GetDepthStencilSurface(&pStencilSurface)))
    {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(pStencilSurface->GetDesc(&desc)))
        {
            m_ZBpp = DepthBitPerPixelFromFormat(desc.Format, &m_StencilBpp);
        }
        SAFERELEASE(pStencilSurface);
    }

    // Set up default render states
    if (!SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, TRUE) ||
        !SetRenderState(VXRENDERSTATE_LOCALVIEWER, TRUE) ||
        !SetRenderState(VXRENDERSTATE_COLORVERTEX, FALSE))
    {
        // Handle setup failure
        SAFERELEASE(m_Device);
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Finish initialization
    UpdateDirectXData();
    FlushCaches();
    UpdateObjectArrays(m_Owner);
    ClearStreamCache();

    if (m_Fullscreen)
        m_Owner->m_FullscreenContext = this;

    m_InCreateDestroy = FALSE;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    if (m_InCreateDestroy)
        return FALSE;

    EndScene();
    ReleaseScreenBackup();

    if ((Flags & VX_RESIZE_NOMOVE) == 0)
    {
        m_PosX = PosX;
        m_PosY = PosY;
    }

    RECT rect;
    if ((Flags & VX_RESIZE_NOSIZE) == 0)
    {
        if (Width == 0 || Height == 0)
        {
            GetClientRect((HWND)m_Window, &rect);
            Width = rect.right - m_PosX;
            Height = rect.bottom - m_PosY;
        }
        m_PresentParams.BackBufferWidth = Width;
        m_PresentParams.BackBufferHeight = Height;

        ReleaseVertexDeclarations();
        ReleaseStateBlocks();
        FlushNonManagedObjects();
        ClearStreamCache();

        if (m_Antialias == 0)
        {
            m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        }
        else if (m_PresentParams.MultiSampleType == D3DMULTISAMPLE_NONE)
        {
            m_PresentParams.MultiSampleType = (m_Antialias < 2 || m_Antialias > 16) ? D3DMULTISAMPLE_2_SAMPLES : (D3DMULTISAMPLE_TYPE)m_Antialias;
            for (int type = m_PresentParams.MultiSampleType; type >= D3DMULTISAMPLE_2_SAMPLES; --type)
            {
                if (SUCCEEDED(m_Owner->m_D3D9->CheckDeviceMultiSampleType(
                        static_cast<CKDX9RasterizerDriver *>(m_Driver)->m_AdapterIndex,
                        D3DDEVTYPE_HAL,
                        m_PresentParams.BackBufferFormat,
                        m_PresentParams.Windowed,
                        m_PresentParams.MultiSampleType,
                        NULL)))
                    break;
                m_PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)type;
            }

            if (m_PresentParams.MultiSampleType < D3DMULTISAMPLE_2_SAMPLES)
                m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        }

        HRESULT hr = m_Device->Reset(&m_PresentParams);
        if (hr == D3DERR_DEVICELOST)
        {
            if (m_Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            {
                hr = m_Device->Reset(&m_PresentParams);
                if (FAILED(hr))
                {
                    m_PresentParams.BackBufferWidth = m_Width;
                    m_PresentParams.BackBufferHeight = m_Height;
                    hr = m_Device->Reset(&m_PresentParams);
                }
            }
        }
        else
        {
            m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
            hr = m_Device->Reset(&m_PresentParams);
        }

        if (SUCCEEDED(hr))
        {
            m_Width = Width;
            m_Height = Height;
        }

        UpdateDirectXData();
        FlushCaches();

        return SUCCEEDED(hr);
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT *rects)
{
    if (m_InCreateDestroy)
        return FALSE;

    DWORD flags = 0;
    if (m_Device)
    {
        if (!m_TransparentMode && (Flags & CKRST_CTXCLEAR_COLOR) != 0 && m_Bpp != 0)
            flags = D3DCLEAR_TARGET;
        if ((Flags & CKRST_CTXCLEAR_STENCIL) != 0 && m_StencilBpp != 0)
            flags |= D3DCLEAR_STENCIL;
        if ((Flags & CKRST_CTXCLEAR_DEPTH) != 0 && m_ZBpp != 0)
            flags |= D3DCLEAR_ZBUFFER;
        return SUCCEEDED(m_Device->Clear(RectCount, (D3DRECT *)rects, flags, Ccol, Z, Stencil));
    }

    return flags == 0;
}

#if LOGGING && LOG_LOADTEXTURE
static int texture_used[100] = {0};
#endif
CKBOOL CKDX9RasterizerContext::BackToFront(CKBOOL vsync)
{
    if (m_InCreateDestroy || !m_Device)
        return FALSE;

    if (m_SceneBegined)
        EndScene();

    HRESULT hr;
    D3DRASTER_STATUS status;
    if (m_CurrentTextureIndex == 0 && vsync && !m_Fullscreen)
    {
        status.InVBlank = FALSE;
        hr = m_Device->GetRasterStatus(0, &status);
        while (SUCCEEDED(hr) && !status.InVBlank)
        {
            hr = m_Device->GetRasterStatus(0, &status);
        }
    }

#ifdef USE_D3D9EX
    hr = m_Device->PresentEx(NULL, NULL, NULL, NULL, D3DPRESENT_INTERVAL_DEFAULT);
#else
    hr = m_Device->Present(NULL, NULL, NULL, D3DPRESENT_INTERVAL_DEFAULT);
#endif
    if (FAILED(hr))
    {
        if (hr == D3DERR_DEVICELOST)
        {
            hr = m_Device->TestCooperativeLevel();
            if (hr == D3DERR_DEVICENOTRESET)
            {
                if (!Resize(m_PosX, m_PosY, m_Width, m_Height))
                {
                    // Handle resize failure
                    return FALSE;
                }
            }
            else
            {
                // Device still lost, can't be reset yet
                return FALSE;
            }
        }
        else
        {
            // Other presentation error
            return FALSE;
        }
    }

#if LOGGING && LOG_LOADTEXTURE
    int count = 0;
    for (int i = 0; i < 100; ++i)
    {
        if (texture_used[i])
        {
            fprintf(stderr, "%d ", i);
            ++count;
        }
    }
    fprintf(stderr, "\n count %d\n", count);
    fprintf(stderr, "buffer swap\n");
    ZeroMemory(texture_used, 100 * sizeof(int));
#endif
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

    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::BeginScene()
{
#ifdef TRACY_ENABLE
    FrameMark;
#endif
    if (m_SceneBegined)
        return TRUE;

    HRESULT hr = m_Device->BeginScene();
    m_SceneBegined = TRUE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::EndScene()
{
    if (!m_SceneBegined)
        return TRUE;

    HRESULT hr = m_Device->EndScene();
    m_SceneBegined = FALSE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    if (data && Light < 128)
        m_CurrentLightData[Light] = *data;

    D3DLIGHT9 light;
    light.Type = (D3DLIGHTTYPE)data->Type;
    light.Range = data->Range;
    light.Attenuation0 = data->Attenuation0;
    light.Attenuation1 = data->Attenuation1;
    light.Attenuation2 = data->Attenuation2;
    light.Ambient.a = data->Ambient.a;
    light.Ambient.r = data->Ambient.r;
    light.Ambient.g = data->Ambient.g;
    light.Ambient.b = data->Ambient.b;
    light.Diffuse.a = data->Diffuse.a;
    light.Diffuse.r = data->Diffuse.r;
    light.Diffuse.g = data->Diffuse.g;
    light.Diffuse.b = data->Diffuse.b;
    light.Position.x = data->Position.x;
    light.Position.y = data->Position.y;
    light.Position.z = data->Position.z;
    light.Direction.x = data->Direction.x;
    light.Direction.y = data->Direction.y;
    light.Direction.z = data->Direction.z;
    light.Falloff = data->Falloff;
    light.Specular.a = data->Specular.a;
    light.Specular.r = data->Specular.r;
    light.Specular.g = data->Specular.g;
    light.Specular.b = data->Specular.b;
    light.Theta = data->InnerSpotCone;
    light.Phi = data->OuterSpotCone;

    if (data->Type == VX_LIGHTPARA)
    {
        light.Type = D3DLIGHT_POINT;
    }
    else if (data->Type == VX_LIGHTSPOT)
    {
        if (light.Theta > PI)
            light.Theta = PI;
        if (light.Phi < light.Theta)
            light.Phi = light.Theta;
    }

    ConvertAttenuationModelFromDX5(light.Attenuation0, light.Attenuation1, light.Attenuation2, data->Range);
    return SUCCEEDED(m_Device->SetLight(Light, &light));
}

CKBOOL CKDX9RasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    return SUCCEEDED(m_Device->LightEnable(Light, Enable));
}

CKBOOL CKDX9RasterizerContext::SetMaterial(CKMaterialData *mat)
{
    if (mat)
        m_CurrentMaterialData = *mat;
    return SUCCEEDED(m_Device->SetMaterial((D3DMATERIAL9 *)mat));
}

CKBOOL CKDX9RasterizerContext::SetViewport(CKViewportData *data)
{
    m_ViewportData = *data;
    return SUCCEEDED(m_Device->SetViewport((D3DVIEWPORT9 *)&m_ViewportData));
}

CKBOOL CKDX9RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    CKDWORD matrixMask = 0;
    D3DTRANSFORMSTATETYPE type = (D3DTRANSFORMSTATETYPE)Type;
    switch (Type)
    {
        case VXMATRIX_WORLD:
            m_WorldMatrix = Mat;
            matrixMask = WORLD_TRANSFORM;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate &= ~WORLD_TRANSFORM;
            type = D3DTS_WORLD;
            break;
        case VXMATRIX_VIEW:
            m_ViewMatrix = Mat;
            matrixMask = VIEW_TRANSFORM;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate = 0;
            type = D3DTS_VIEW;
            break;
        case VXMATRIX_PROJECTION:
            m_ProjectionMatrix = Mat;
            matrixMask = PROJ_TRANSFORM;
            m_MatrixUptodate = 0;
            type = D3DTS_PROJECTION;
            break;
        case VXMATRIX_TEXTURE0:
        case VXMATRIX_TEXTURE1:
        case VXMATRIX_TEXTURE2:
        case VXMATRIX_TEXTURE3:
        case VXMATRIX_TEXTURE4:
        case VXMATRIX_TEXTURE5:
        case VXMATRIX_TEXTURE6:
        case VXMATRIX_TEXTURE7:
            matrixMask = TEXTURE0_TRANSFORM << (Type - TEXTURE1_TRANSFORM);
            break;
        default:
            return FALSE;
    }
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & matrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= matrixMask;
    }
    else
    {
        m_UnityMatrixMask &= ~matrixMask;
    }
    return SUCCEEDED(m_Device->SetTransform(type, (const D3DMATRIX *)&Mat));
}

CKBOOL CKDX9RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    if (m_StateCache[State].Flags != 0)
        return TRUE;

    if (m_StateCache[State].Valid && m_StateCache[State].Value == Value)
    {
        ++m_RenderStateCacheHit;
        return TRUE;
    }

    ++m_RenderStateCacheMiss;
    m_StateCache[State].Value = Value;
    m_StateCache[State].Valid = TRUE;

    if (State < m_StateCacheMissMask.Size() && m_StateCacheMissMask.IsSet(State))
        return FALSE;

    if (State < m_StateCacheHitMask.Size() && m_StateCacheHitMask.IsSet(State))
    {
        static D3DCULL VXCullModes[4] = {D3DCULL_NONE, D3DCULL_NONE, D3DCULL_CW, D3DCULL_CCW};
        static D3DCULL VXCullModesInverted[4] = {D3DCULL_NONE, D3DCULL_NONE, D3DCULL_CCW, D3DCULL_CW};

        if (State == VXRENDERSTATE_CULLMODE)
        {
            if (!m_InverseWinding)
                return SUCCEEDED(m_Device->SetRenderState(D3DRS_CULLMODE, VXCullModes[Value]));
            else
                return SUCCEEDED(m_Device->SetRenderState(D3DRS_CULLMODE, VXCullModesInverted[Value]));
        }
        if (State == VXRENDERSTATE_INVERSEWINDING)
        {
            m_InverseWinding = Value != 0;
            m_StateCache[VXRENDERSTATE_CULLMODE].Valid = FALSE;
        }
        return TRUE;
    }

    return SUCCEEDED(m_Device->SetRenderState((D3DRENDERSTATETYPE)State, Value));
}

CKBOOL CKDX9RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    if (m_StateCache[State].Valid)
    {
        *Value = m_StateCache[State].Value;
        return TRUE;
    }
    else
    {
        *Value = m_StateCache[State].DefaultValue;
        if (State == VXRENDERSTATE_INVERSEWINDING)
        {
            *Value = m_InverseWinding;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}

CKBOOL CKDX9RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
#if LOGGING && LOG_SETTEXTURE
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
    HRESULT hr = E_FAIL;
    CKDX9TextureDesc *desc = NULL;
    if (Texture != 0 && Texture < m_Textures.Size() &&
        (desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture])) != NULL && desc->DxTexture != NULL)
    {
        hr = m_Device->SetTexture(Stage, desc->DxTexture);
        if (Stage == 0)
        {
            m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        }
    }
    else
    {
        hr = m_Device->SetTexture(Stage, NULL);
        if (Stage == 0)
        {
            m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        }
    }

    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSV, Value);
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSW, Value);
            break;
        case CKRST_TSS_ADDRESSU:
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
            break;
        case CKRST_TSS_ADDRESSV:
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSV, Value);
            break;
        case CKRST_TSS_BORDERCOLOR:
        case CKRST_TSS_MIPMAPLODBIAS:
        case CKRST_TSS_MAXMIPMLEVEL:
        case CKRST_TSS_MAXANISOTROPY:
            m_Device->SetSamplerState(Stage, (D3DSAMPLERSTATETYPE)(Tss - (CKRST_TSS_BORDERCOLOR - D3DSAMP_BORDERCOLOR)), Value);
            break;
        case CKRST_TSS_MAGFILTER:
            if (m_PresentInterval == 0)
            {
                LPDIRECT3DSTATEBLOCK9 block = m_TextureMagFilterStateBlocks[Value][Stage];
                if (block)
                {
                    HRESULT hr = block->Apply();
#if LOGGING && LOG_SETTEXURESTAGESTATE
                    fprintf(stderr, "Applying TextureMagFilterStateBlocks Value %d Stage %d -> 0x%x\n", Value, Stage, hr);
#endif
                    return SUCCEEDED(hr);
                }
                switch (Value)
                {
                    case VXTEXTUREFILTER_NEAREST:
                    case VXTEXTUREFILTER_MIPNEAREST:
                        m_Device->SetSamplerState(Stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                        break;
                    case VXTEXTUREFILTER_LINEAR:
                    case VXTEXTUREFILTER_MIPLINEAR:
                    case VXTEXTUREFILTER_LINEARMIPNEAREST:
                    case VXTEXTUREFILTER_LINEARMIPLINEAR:
                        m_Device->SetSamplerState(Stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                        break;
                    case VXTEXTUREFILTER_ANISOTROPIC:
                        m_Device->SetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, D3DTEXF_LINEAR);
                        m_Device->SetSamplerState(Stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
                        break;
                    default:
                        break;
                }
            }
            else
            {
                m_Device->SetSamplerState(Stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                return FALSE;
            }
            break;
        case CKRST_TSS_MINFILTER:
            if (m_PresentInterval == 0 && m_CurrentPresentInterval == 0)
            {
                LPDIRECT3DSTATEBLOCK9 block = m_TextureMinFilterStateBlocks[Value][Stage];
                if (block)
                {
                    HRESULT hr = block->Apply();
#if LOGGING && LOG_SETTEXURESTAGESTATE
                    fprintf(stderr, "Applying TextureMinFilterStateBlocks Value %d Stage %d -> 0x%x\n", Value, Stage, hr);
#endif
                    return SUCCEEDED(hr);
                }
            }

            switch (Value)
            {
                case VXTEXTUREFILTER_NEAREST:
                    m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_LINEAR:
                    if (m_PresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_MIPNEAREST:
                    m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    if (m_CurrentPresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_MIPLINEAR:
                    if (m_PresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    if (m_CurrentPresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_LINEARMIPNEAREST:
                    m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    if (m_CurrentPresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_LINEARMIPLINEAR:
                    if (m_PresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    if (m_CurrentPresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                    break;
                case VXTEXTUREFILTER_ANISOTROPIC:
                    if (m_PresentInterval == 0)
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
                    else
                        m_Device->SetSamplerState(Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                    m_Device->SetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, D3DTEXF_LINEAR);
                    break;
                default:
                    break;
            }
            break;
        case CKRST_TSS_TEXTUREMAPBLEND:
            {
                LPDIRECT3DSTATEBLOCK9 block = m_TextureMapBlendStateBlocks[Value][Stage];
                if (block)
                {
                    HRESULT hr = block->Apply();
#if LOGGING && LOG_SETTEXURESTAGESTATE
                    fprintf(stderr, "Applying TextureMapBlendStateBlocks Value %d Stage %d -> 0x%x\n", Value, Stage, hr);
#endif
                    return SUCCEEDED(hr);
                }
            }

            switch (Value)
            {
                case VXTEXTUREBLEND_DECAL:
                case VXTEXTUREBLEND_COPY:
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    break;
                case VXTEXTUREBLEND_MODULATE:
                case VXTEXTUREBLEND_MODULATEALPHA:
                case VXTEXTUREBLEND_MODULATEMASK:
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                    break;
                case VXTEXTUREBLEND_DECALALPHA:
                case VXTEXTUREBLEND_DECALMASK:
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                    break;
                case VXTEXTUREBLEND_ADD:
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_ADD);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                    break;
                case VXTEXTUREBLEND_DOTPRODUCT3:
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                    break;
                default:
                    break;
            }
            break;
        case CKRST_TSS_STAGEBLEND:
            if (Value <= 0xFF)
            {
                if (Value == 0)
                {
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                    return TRUE;
                }

                CKStageBlend *blend = m_Owner->m_BlendStages[Value];
                if (!blend)
                    return FALSE;

                m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, blend->Cop);
                m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG1, blend->Carg1);
                m_Device->SetTextureStageState(Stage, D3DTSS_COLORARG2, blend->Carg2);
                m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, blend->Aop);
                m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, blend->Aarg1);
                m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG2, blend->Aarg2);

                if (FAILED(m_Device->ValidateDevice((DWORD *)&Stage)))
                {
                    m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                }
            }
            else
            {
                return FALSE;
            }
            break;
        default:
            return SUCCEEDED(m_Device->SetTextureStageState(Stage, (D3DTEXTURESTAGESTATETYPE)Tss, Value));
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    if (VShaderIndex >= m_VertexShaders.Size())
        return FALSE;

    CKVertexShaderDesc *desc = m_VertexShaders[VShaderIndex];
    m_CurrentVertexShaderCache = VShaderIndex;
    m_CurrentVertexFormatCache = 0;
    return desc != NULL;
}

CKBOOL CKDX9RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    if (PShaderIndex >= m_PixelShaders.Size())
        return FALSE;

    CKDX9PixelShaderDesc *desc = static_cast<CKDX9PixelShaderDesc *>(m_PixelShaders[PShaderIndex]);
    if (!desc)
        return FALSE;

    return SUCCEEDED(m_Device->SetPixelShader(desc->DxShader));
}

CKBOOL CKDX9RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return SUCCEEDED(m_Device->SetVertexShaderConstantF(Register, static_cast<const float *>(Data), CstCount));
}

CKBOOL CKDX9RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return SUCCEEDED(m_Device->SetPixelShaderConstantF(Register, static_cast<const float *>(Data), CstCount));
}

CKBOOL CKDX9RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, WORD *indices, int indexcount, VxDrawPrimitiveData *data)
{
#if LOGGING && LOG_DRAWPRIMITIVE
    fprintf(stderr, "drawprimitive ib %d\n", indexcount);
#endif
#if STEP
    if (step_mode)
    {
        BackToFront(false);
        _getch();
    }
#endif
#if LOG_BATCHSTATS
    ++directbat;
#endif

#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (!m_SceneBegined)
        BeginScene();

    CKBOOL clip = FALSE;
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    if ((data->Flags & CKRST_DP_DOCLIP))
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, TRUE);
        clip = TRUE;
    }
    else
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, FALSE);
    }

    CKDWORD index = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    CKDX9VertexBufferDesc *vertexBufferDesc = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[index]);
    if (vertexBufferDesc == NULL)
        return FALSE;

    void *ppbData = NULL;
    HRESULT hr = D3DERR_INVALIDCALL;
    CKDWORD startIndex = 0;
    if (vertexBufferDesc->m_CurrentVCount + data->VertexCount <= vertexBufferDesc->m_MaxVertexCount)
    {
#ifdef TRACY_ENABLE
        ZoneScopedN("Lock");
#endif
        hr = vertexBufferDesc->DxBuffer->Lock(vertexSize * vertexBufferDesc->m_CurrentVCount,
                                              vertexSize * data->VertexCount, &ppbData, D3DLOCK_NOOVERWRITE);
        startIndex = vertexBufferDesc->m_CurrentVCount;
        vertexBufferDesc->m_CurrentVCount += data->VertexCount;
    }
    else
    {
#ifdef TRACY_ENABLE
        ZoneScopedN("Lock");
#endif
        hr = vertexBufferDesc->DxBuffer->Lock(0, vertexSize * data->VertexCount, &ppbData, D3DLOCK_DISCARD);
        vertexBufferDesc->m_CurrentVCount = data->VertexCount;
    }
    if (FAILED(hr))
        return FALSE;

    CKRSTLoadVertexBuffer(reinterpret_cast<CKBYTE *>(ppbData), vertexFormat, vertexSize, data);
    hr = vertexBufferDesc->DxBuffer->Unlock();
    assert(SUCCEEDED(hr));

    return InternalDrawPrimitiveVB(pType, vertexBufferDesc, startIndex, data->VertexCount, indices, indexcount, clip);
}

CKBOOL CKDX9RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
                                               CKDWORD VertexCount, WORD *indices, int indexcount)
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

#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (VertexBuffer >= m_VertexBuffers.Size())
        return FALSE;

    CKVertexBufferDesc *vertexBufferDesc = m_VertexBuffers[VertexBuffer];
    if (vertexBufferDesc == NULL)
        return FALSE;

    if (!m_SceneBegined)
        BeginScene();

    return InternalDrawPrimitiveVB(pType, static_cast<CKDX9VertexBufferDesc *>(vertexBufferDesc), StartIndex,
                                   VertexCount, indices, indexcount, TRUE);
}

CKBOOL CKDX9RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
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

#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vertexBufferDesc = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (vertexBufferDesc == NULL)
        return FALSE;

    CKDX9IndexBufferDesc *indexBufferDesc = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (indexBufferDesc == NULL)
        return FALSE;

    SetupStreams(vertexBufferDesc->DxBuffer, vertexBufferDesc->m_VertexFormat, vertexBufferDesc->m_VertexSize);

    switch (pType)
    {
        case VX_LINELIST:
            Indexcount = Indexcount / 2;
            break;
        case VX_LINESTRIP:
            Indexcount = Indexcount - 1;
            break;
        case VX_TRIANGLELIST:
            Indexcount = Indexcount / 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            Indexcount = Indexcount - 2;
            break;
        default:
            break;
    }

    m_Device->SetIndices(indexBufferDesc->DxBuffer);

    return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, MinVIndex, 0, VertexCount, StartIndex, Indexcount));
}

CKBOOL CKDX9RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    CKBOOL result = FALSE;

    if (ObjIndex >= m_Textures.Size())
        return FALSE;

    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
            {
                result = CreateSprite(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
#if LOGGING
                CKSpriteDesc *desc = m_Sprites[ObjIndex];
                fprintf(stderr, "idx: %d\n", ObjIndex);
                for (auto it = desc->Textures.Begin(); it != desc->Textures.End(); ++it)
                {
                    fprintf(stderr, "(%d,%d) WxH: %dx%d, SWxSH: %dx%d\n", it->x, it->y, it->w, it->h, it->sw, it->sh);
                }
                fprintf(stderr, "---\n");
#endif
                break;
            }
        case CKRST_OBJ_VERTEXBUFFER:
            result = CreateVertexBuffer(ObjIndex, static_cast<CKVertexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_INDEXBUFFER:
            result = CreateIndexBuffer(ObjIndex, static_cast<CKIndexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXSHADER:
            result = CreateVertexShader(ObjIndex, static_cast<CKVertexShaderDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_PIXELSHADER:
            result = CreatePixelShader(ObjIndex, static_cast<CKPixelShaderDesc *>(DesiredFormat));
            break;
        default:
            return FALSE;
    }

    return result;
}

CKBOOL CKDX9RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
#if LOGGING && LOG_LOADTEXTURE
    texture_used[Texture] = 1;
    fprintf(stderr, "load texture %d %dx%d %d\n", Texture, SurfDesc.Width, SurfDesc.Height, miplevel);
#endif
    if (Texture >= m_Textures.Size())
        return FALSE;

    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxTexture)
        return FALSE;

    // Skip special texture types (cubemaps and render targets are handled separately)
    if ((desc->Flags & (CKRST_TEXTURE_CUBEMAP | CKRST_TEXTURE_RENDERTARGET)) != 0)
        return TRUE;

    // Determine target mip level
    int actualMipLevel = (miplevel < 0) ? 0 : miplevel;

    D3DSURFACE_DESC surfaceDesc;
    HRESULT hr = desc->DxTexture->GetLevelDesc(actualMipLevel, &surfaceDesc);
    if (FAILED(hr))
        return FALSE;

    // Special handling for compressed textures (DXT formats)
    bool isCompressedFormat = 
        (surfaceDesc.Format == D3DFMT_DXT1 || 
         surfaceDesc.Format == D3DFMT_DXT2 || 
         surfaceDesc.Format == D3DFMT_DXT3 ||
         surfaceDesc.Format == D3DFMT_DXT4 || 
         surfaceDesc.Format == D3DFMT_DXT5);

    if (isCompressedFormat && (D3DXLoadSurfaceFromSurface && D3DXLoadSurfaceFromMemory))
    {
        // Use D3DX functions for compressed textures
        IDirect3DSurface9 *pSurfaceLevel = NULL;
        hr = desc->DxTexture->GetSurfaceLevel(actualMipLevel, &pSurfaceLevel);
        if (FAILED(hr) || !pSurfaceLevel)
        {
            SAFERELEASE(pSurfaceLevel);
            return FALSE;
        }

        // Prepare source rect
        RECT srcRect{0, 0, SurfDesc.Height, SurfDesc.Width};
        D3DFORMAT format = VxPixelFormatToD3DFormat(VxImageDesc2PixelFormat(SurfDesc));

        // Load texture data
        hr = D3DXLoadSurfaceFromMemory(
            pSurfaceLevel,      // Destination surface
            NULL,               // No palette
            NULL,               // Full destination surface
            SurfDesc.Image,     // Source data
            format,             // Source format
            SurfDesc.BytesPerLine, // Source pitch
            NULL,               // No palette
            &srcRect,           // Source rect
            D3DX_FILTER_LINEAR, // Filter
            0);                 // No color key
        if (FAILED(hr))
        {
            SAFERELEASE(pSurfaceLevel);
            return FALSE;
        }

         // Generate mipmaps if requested
         if (miplevel == -1 && desc->MipMapCount > 0)
         {
            // Generate each mipmap level from the base level using box filter
            for (int i = 1; i <= desc->MipMapCount; ++i)
            {
                IDirect3DSurface9 *pMipSurface = NULL;
                hr = desc->DxTexture->GetSurfaceLevel(i, &pMipSurface);
                
                if (SUCCEEDED(hr) && pMipSurface)
                {
                    // Generate this mip level from the base level
                    hr = D3DXLoadSurfaceFromSurface(
                        pMipSurface,        // Destination
                        NULL,               // No palette
                        NULL,               // Full surface
                        pSurfaceLevel,      // Source (base level)
                        NULL,               // No palette
                        NULL,               // Full surface
                        D3DX_FILTER_BOX,    // Box filter for better quality
                        0);                 // No color key
                    
                    SAFERELEASE(pMipSurface);
                    
                    if (FAILED(hr))
                    {
                        // Log but continue if a mip level fails
#if LOGGING && LOG_LOADTEXTURE
                        fprintf(stderr, "Failed to generate mipmap level %d, hr=0x%x\n", i, hr);
#endif
                    }
                }
            }
        }

        SAFERELEASE(pSurfaceLevel);
        return TRUE;
    }

    // Standard texture loading path (non-compressed)

    // Allocate memory for mipmap generation if needed
    VxImageDescEx src = SurfDesc;
    VxImageDescEx dst;
    CKBYTE *mipmapBuffer = NULL;
    bool needMipmapGen = (miplevel == -1 && desc->MipMapCount > 0);

    if (needMipmapGen)
    {
        mipmapBuffer = m_Owner->AllocateObjects(surfaceDesc.Width * surfaceDesc.Height);
        if (!mipmapBuffer)
            return FALSE;

        // If source size doesn't match texture size, we need to rescale
        if (surfaceDesc.Width != src.Width || surfaceDesc.Height != src.Height)
        {
            dst.Width = src.Width;
            dst.Height = src.Height;
            dst.BitsPerPixel = 32;
            dst.BytesPerLine = 4 * dst.Width;
            dst.AlphaMask = A_MASK;
            dst.RedMask = R_MASK;
            dst.GreenMask = G_MASK;
            dst.BlueMask = B_MASK;
            dst.Image = mipmapBuffer;

            // Convert the source to our working format
            VxDoBlit(src, dst);
            src = dst;
        }
    }

    // Lock and load base level
    D3DLOCKED_RECT lockRect;
    hr = desc->DxTexture->LockRect(actualMipLevel, &lockRect, NULL, 0);
    if (FAILED(hr))
    {
        return FALSE;
    }

    // Copy data to texture surface
    if (!LoadSurface(surfaceDesc, lockRect, src))
    {
        // Unlock before returning
        desc->DxTexture->UnlockRect(actualMipLevel);
        return FALSE;
    }

    // Unlock the base level
    hr = desc->DxTexture->UnlockRect(actualMipLevel);
    if (FAILED(hr))
    {
        return FALSE;
    }

    // Generate mipmaps if needed
    if (needMipmapGen && mipmapBuffer)
    {
        // Start with the source image data
        dst = src;

        // Generate each mip level
        for (int i = 1; i <= desc->MipMapCount; ++i)
        {
            // Generate the next mip level
            VxGenerateMipMap(dst, mipmapBuffer);

            // Calculate dimensions of this mip level
            if (dst.Width > 1)
                dst.Width >>= 1;
            if (dst.Height > 1)
                dst.Height >>= 1;
            dst.BytesPerLine = 4 * dst.Width;
            dst.Image = mipmapBuffer;

            // Check if surface exists at this mip level
            if (FAILED(desc->DxTexture->GetLevelDesc(i, &surfaceDesc)))
            {
                break; // No more mip levels
            }

            // Lock the mip level
            hr = desc->DxTexture->LockRect(i, &lockRect, NULL, 0);
            if (FAILED(hr))
            {
                break; // Can't lock, but don't fail the whole operation
            }

            // Copy data to the mip level
            if (!LoadSurface(surfaceDesc, lockRect, dst))
            {
                desc->DxTexture->UnlockRect(i);
                break;
            }

            // Unlock the mip level
            hr = desc->DxTexture->UnlockRect(i);
            if (FAILED(hr))
            {
                break; // Failed to unlock, but don't fail the whole operation
            }

            // Stop if we've reached smallest mip level
            if (dst.Width <= 1 && dst.Height <= 1)
            {
                break;
            }
        }
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
#if LOGGING && LOG_COPYTEXTURE
    fprintf(stderr, "copy to texture %d (%f,%f,%f,%f) (%f,%f,%f,%f)\n", Texture,
            Src->left, Src->top, Src->right, Src->bottom,
            Dest->left, Dest->top, Dest->right, Dest->bottom);
#endif
    if (Texture >= m_Textures.Size())
        return FALSE;

    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxTexture)
        return FALSE;

    // Create destination rectangle
    RECT destRect;
    if (Dest)
        SetRect(&destRect, (int)Dest->left, (int)Dest->top, (int)Dest->right, (int)Dest->bottom);
    else
        SetRect(&destRect, 0, 0, desc->Format.Width, desc->Format.Height);

    // Create source rectangle
    RECT srcRect;
    if (Src)
        SetRect(&srcRect, (int)Src->left, (int)Src->top, (int)Src->right, (int)Src->bottom);
    else
        SetRect(&srcRect, 0, 0, desc->Format.Width, desc->Format.Height);

    HRESULT hr = S_OK;
    IDirect3DSurface9 *backBuffer = NULL;
    IDirect3DSurface9 *textureSurface = NULL;
    CKBOOL success = FALSE;

    // Get back buffer
    hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (FAILED(hr) || !backBuffer)
    {
        return FALSE;
    }

    // Get texture surface
    hr = desc->DxTexture->GetSurfaceLevel(0, &textureSurface);
    if (FAILED(hr) || !textureSurface)
    {
        SAFERELEASE(backBuffer);
        return FALSE;
    }

    // Define destination point
    POINT pt = {destRect.left, destRect.top};

    // Copy from back buffer to texture surface
    // Note: UpdateSurface param order: source, source rect, destination, destination point
    hr = m_Device->UpdateSurface(backBuffer, &srcRect, textureSurface, &pt);
    if (SUCCEEDED(hr))
    {
        success = TRUE;
    }
    // If copying failed and this is a managed texture, try recreating as a render target
    else if (desc->Flags & CKRST_TEXTURE_MANAGED)
    {
        // Release current texture
        SAFERELEASE(textureSurface);
        SAFERELEASE(desc->DxTexture);

        // Create new texture as render target
        hr = m_Device->CreateTexture(
            desc->Format.Width,
            desc->Format.Height,
            1,
            D3DUSAGE_RENDERTARGET,
            m_PresentParams.BackBufferFormat,
            D3DPOOL_DEFAULT,
            &desc->DxTexture,
            NULL
        );
        if (SUCCEEDED(hr) && desc->DxTexture)
        {
            // Update texture format info
            D3DFormatToTextureDesc(m_PresentParams.BackBufferFormat, desc);
            desc->Flags &= ~CKRST_TEXTURE_MANAGED;
            desc->Flags |= (CKRST_TEXTURE_RENDERTARGET | CKRST_TEXTURE_VALID);

            // Get the new surface
            hr = desc->DxTexture->GetSurfaceLevel(0, &textureSurface);
            if (SUCCEEDED(hr) && textureSurface)
            {
                // Try copy again
                hr = m_Device->UpdateSurface(backBuffer, &srcRect, textureSurface, &pt);
                success = SUCCEEDED(hr);
            }
        }
    }

    // Clean up resources
    SAFERELEASE(textureSurface);
    SAFERELEASE(backBuffer);

    return success;
}

CKBOOL CKDX9RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face)
{
    // End any current scene
    EndScene();

    // Case 1: Restoring the default render target
    if (!TextureObject)
    {
        if (!m_DefaultBackBuffer)
            return FALSE;
        
        HRESULT hr = m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
        if (SUCCEEDED(hr))
        {
            // Restore default depth buffer
            if (m_DefaultDepthBuffer)
            {
                m_Device->SetDepthStencilSurface(m_DefaultDepthBuffer);
            }
            
            // Reset the current texture's flags
            if (m_CurrentTextureIndex < m_Textures.Size())
            {
                CKTextureDesc *desc = m_Textures[m_CurrentTextureIndex];
                if (desc)
                {
                    desc->Flags &= ~CKRST_TEXTURE_RENDERTARGET;
                }
            }
            
            // Release the saved buffers
            SAFERELEASE(m_DefaultBackBuffer);
            SAFERELEASE(m_DefaultDepthBuffer);
            m_CurrentTextureIndex = 0;
            
            return TRUE;
        }
        else
        {
            // Failed to set default render target, keep it for retry later
            return FALSE;
        }
    }

    // Input validation
    if (TextureObject >= m_Textures.Size() || !m_Device || m_DefaultBackBuffer)
        return FALSE;

    // Handle cube map case
    CKBOOL cubemap = FALSE;
    if (Height < 0)
    {
        cubemap = TRUE;
        Height = Width;
    }

    // Get or create the texture descriptor
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[TextureObject]);
    if (!desc)
    {
        desc = new CKDX9TextureDesc;
        desc->Format.Width = (Width != 0) ? Width : 256;
        desc->Format.Height = (Height != 0) ? Height : 256;
        m_Textures[TextureObject] = desc;
    }

    // Capture current render target and depth buffer
    HRESULT hr = m_Device->GetRenderTarget(0, &m_DefaultBackBuffer);
    if (FAILED(hr) || !m_DefaultBackBuffer)
        return FALSE;

    hr = m_Device->GetDepthStencilSurface(&m_DefaultDepthBuffer);
    if (FAILED(hr))
    {
        // No depth buffer is ok, but we should clear the pointer
        m_DefaultDepthBuffer = NULL;
    }

    // Unbind all textures to avoid circular dependencies
    for (int i = 0; i < m_Driver->m_3DCaps.MaxNumberTextureStage; ++i)
    {
        m_Device->SetTexture(i, NULL);
    }

    // Try to use existing texture as render target
    CKBOOL surfaceSuccess = FALSE;
    IDirect3DSurface9 *surface = NULL;
    
    if ((cubemap || desc->DxRenderTexture) && desc->DxTexture)
    {
        if (cubemap)
        {
            D3DRESOURCETYPE type = desc->DxTexture->GetType();
            if (type == D3DRTYPE_CUBETEXTURE)
            {
                hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, 0, &surface);
                if (SUCCEEDED(hr) && surface)
                {
                    D3DSURFACE_DESC surfaceDesc;
                    hr = surface->GetDesc(&surfaceDesc);
                    
                    if (SUCCEEDED(hr) && (surfaceDesc.Usage & D3DUSAGE_RENDERTARGET))
                    {
                        surfaceSuccess = TRUE;
                    }
                }
            }
        }
        else
        {
            desc->DxRenderTexture = desc->DxTexture;
            hr = desc->DxTexture->GetSurfaceLevel(0, &surface);
            
            if (SUCCEEDED(hr) && surface)
            {
                D3DSURFACE_DESC surfaceDesc;
                hr = surface->GetDesc(&surfaceDesc);
                
                if (SUCCEEDED(hr) && (surfaceDesc.Usage & D3DUSAGE_RENDERTARGET))
                {
                    surfaceSuccess = TRUE;
                }
            }
        }
    }
    
    // Create new texture if we couldn't use existing one
    if (!surfaceSuccess)
    {
        // Clean up any surface we might have retrieved
        SAFERELEASE(surface);
        
        // Release previous texture resources
        desc->Flags &= ~CKRST_TEXTURE_VALID;
        SAFERELEASE(desc->DxTexture);
        SAFERELEASE(desc->DxRenderTexture);
        desc->MipMapCount = 0;
        
        // Create appropriate texture type
        if (cubemap)
        {
            hr = m_Device->CreateCubeTexture(
                desc->Format.Width, 
                1, 
                D3DUSAGE_RENDERTARGET, 
                m_PresentParams.BackBufferFormat,
                D3DPOOL_DEFAULT, 
                &desc->DxCubeTexture, 
                NULL
            );
            
            if (FAILED(hr))
            {
                desc->Flags &= ~CKRST_TEXTURE_VALID;
                SAFERELEASE(m_DefaultBackBuffer);
                SAFERELEASE(m_DefaultDepthBuffer);
                return FALSE;
            }
            
            hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, 0, &surface);
            if (FAILED(hr) || !surface)
            {
                desc->Flags &= ~CKRST_TEXTURE_VALID;
                SAFERELEASE(desc->DxCubeTexture);
                SAFERELEASE(m_DefaultBackBuffer);
                SAFERELEASE(m_DefaultDepthBuffer);
                return FALSE;
            }
        }
        else
        {
            hr = m_Device->CreateTexture(
                desc->Format.Width, 
                desc->Format.Height, 
                1, 
                D3DUSAGE_RENDERTARGET,
                m_PresentParams.BackBufferFormat, 
                D3DPOOL_DEFAULT, 
                &desc->DxTexture, 
                NULL
            );
            
            if (FAILED(hr))
            {
                desc->Flags &= ~CKRST_TEXTURE_VALID;
                SAFERELEASE(m_DefaultBackBuffer);
                SAFERELEASE(m_DefaultDepthBuffer);
                return FALSE;
            }
            
            hr = m_Device->CreateTexture(
                desc->Format.Width, 
                desc->Format.Height, 
                1, 
                D3DUSAGE_RENDERTARGET,
                m_PresentParams.BackBufferFormat, 
                D3DPOOL_DEFAULT, 
                &desc->DxRenderTexture, 
                NULL
            );
            
            if (FAILED(hr))
            {
                SAFERELEASE(desc->DxTexture);
                desc->Flags &= ~CKRST_TEXTURE_VALID;
                SAFERELEASE(m_DefaultBackBuffer);
                SAFERELEASE(m_DefaultDepthBuffer);
                return FALSE;
            }
            
            hr = desc->DxRenderTexture->GetSurfaceLevel(0, &surface);
            if (FAILED(hr) || !surface)
            {
                SAFERELEASE(desc->DxTexture);
                SAFERELEASE(desc->DxRenderTexture);
                desc->Flags &= ~CKRST_TEXTURE_VALID;
                SAFERELEASE(m_DefaultBackBuffer);
                SAFERELEASE(m_DefaultDepthBuffer);
                return FALSE;
            }
        }
    }

    // Get appropriate depth buffer
    IDirect3DSurface9 *zbuffer = GetTempZBuffer(desc->Format.Width, desc->Format.Height);
    
    // Set render target and depth buffer
    hr = m_Device->SetRenderTarget(0, surface);
    if (SUCCEEDED(hr))
    {
        // Set the depth-stencil surface if available
        if (zbuffer)
        {
            m_Device->SetDepthStencilSurface(zbuffer);
        }
        
        // Update texture properties
        D3DFormatToTextureDesc(m_PresentParams.BackBufferFormat, desc);
        desc->Flags &= ~CKRST_TEXTURE_MANAGED;
        desc->Flags |= CKRST_TEXTURE_VALID | CKRST_TEXTURE_RENDERTARGET;
        
        if (cubemap)
            desc->Flags |= CKRST_TEXTURE_CUBEMAP;
            
        m_CurrentTextureIndex = TextureObject;
        SAFERELEASE(surface);
        return TRUE;
    }
    else
    {
        // Failed to set render target, clean up
        desc->Flags &= ~CKRST_TEXTURE_VALID;
        SAFERELEASE(surface);
        SAFERELEASE(m_DefaultBackBuffer);
        SAFERELEASE(m_DefaultDepthBuffer);
        return FALSE;
    }
}

CKBOOL CKDX9RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    if (Sprite >= m_Sprites.Size() || !src || !dst)
        return FALSE;

    CKSpriteDesc *sprite = m_Sprites[Sprite];
    if (!sprite || sprite->Textures.IsEmpty())
        return FALSE;

    // Quick boundary checks with early exit
    if (src->GetWidth() <= 0.0f || src->GetHeight() <= 0.0f ||
        dst->GetWidth() <= 0.0f || dst->GetHeight() <= 0.0f ||
        src->right < 0.0f || src->bottom < 0.0f ||
        sprite->Format.Width <= src->left || sprite->Format.Height <= src->top ||
        dst->right < 0.0f || dst->bottom < 0.0f ||
        m_Width <= dst->left || m_Height <= dst->top)
        return FALSE;

    // Begin scene if needed
    if (!m_SceneBegined)
        BeginScene();

    // Set up rendering states once (not per texture)
    HRESULT hr;

    // Save current render states to restore later
    CKDWORD oldZEnable, oldZWriteEnable, oldLighting, oldCullMode;
    GetRenderState(VXRENDERSTATE_ZENABLE, &oldZEnable);
    GetRenderState(VXRENDERSTATE_ZWRITEENABLE, &oldZWriteEnable);
    GetRenderState(VXRENDERSTATE_LIGHTING, &oldLighting);
    GetRenderState(VXRENDERSTATE_CULLMODE, &oldCullMode);

    // Set up texture stage states once
    m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    m_Device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);

    SetRenderState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    SetRenderState(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
    SetRenderState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    SetRenderState(VXRENDERSTATE_ZENABLE, FALSE);
    SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    SetRenderState(VXRENDERSTATE_WRAP0, 0);
    SetRenderState(VXRENDERSTATE_CLIPPING, FALSE);
    SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, FALSE);

    // Save original viewport
    D3DVIEWPORT9 oldViewport;
    m_Device->GetViewport(&oldViewport);

    // Set sprite viewport
    D3DVIEWPORT9 viewport = {0, 0, m_Width, m_Height, 0.0f, 1.0f};
    m_Device->SetViewport(&viewport);

    // Calculate scaling ratios once
    const float widthRatio = dst->GetWidth() / src->GetWidth();
    const float heightRatio = dst->GetHeight() / src->GetHeight();

    // Single-pass texture processing
    // Create a direct list of visible textures with calculated coordinates
    struct VisibleTexture
    {
        CKDWORD TextureId;
        float ScreenLeft, ScreenTop, ScreenRight, ScreenBottom;
        float TexU, TexV, TexU2, TexV2;
    };

    // Use a local stack buffer for speed and to avoid heap allocations
    // for most common use cases
    VisibleTexture localBuffer[16];
    XArray<VisibleTexture> dynamicBuffer;
    VisibleTexture *visibleTextures = localBuffer;
    int visibleCount = 0;
    int maxVisibleCount = 16;

    // First pass: identify visible textures and calculate their coordinates
    for (auto texture = sprite->Textures.Begin(); texture != sprite->Textures.End(); ++texture)
    {
        const float tx = texture->x;
        const float ty = texture->y;
        const float tr = tx + texture->w;
        const float tb = ty + texture->h;

        // Skip textures completely outside the view
        if (tx > src->right || ty > src->bottom || tr < src->left || tb < src->top)
            continue;

        // Ensure we have enough space
        if (visibleCount >= maxVisibleCount)
        {
            // Need to switch to dynamic buffer
            if (dynamicBuffer.IsEmpty())
            {
                // First time switching - copy local buffer to dynamic
                dynamicBuffer.Resize(maxVisibleCount * 2);
                memcpy(dynamicBuffer.Begin(), localBuffer, maxVisibleCount * sizeof(VisibleTexture));
                visibleTextures = dynamicBuffer.Begin();
                maxVisibleCount = dynamicBuffer.Size();
            }
            else if (visibleCount >= maxVisibleCount)
            {
                // Need to resize dynamic buffer
                dynamicBuffer.Resize(maxVisibleCount * 2);
                visibleTextures = dynamicBuffer.Begin();
                maxVisibleCount = dynamicBuffer.Size();
            }
        }

        VisibleTexture &visible = visibleTextures[visibleCount++];
        visible.TextureId = texture->IndexTexture;

        // Calculate texture coordinates, clipping to visible region
        float tuStart = 0.0f;
        float tvStart = 0.0f;
        float tuEnd = static_cast<float>(texture->w) / texture->sw;
        float tvEnd = static_cast<float>(texture->h) / texture->sh;

        // Calculate screen coordinates
        float screenLeft = (tx - src->left) * widthRatio + dst->left;
        float screenTop = (ty - src->top) * heightRatio + dst->top;
        float screenRight = (tr - src->left) * widthRatio + dst->left;
        float screenBottom = (tb - src->top) * heightRatio + dst->top;

        // Handle partial visibility with texture coordinate adjustment
        if (src->right < tr)
        {
            const float clippedWidth = src->right - tx;
            screenRight = (tx + clippedWidth - src->left) * widthRatio + dst->left;
            tuEnd = clippedWidth / texture->sw;
        }

        if (src->bottom < tb)
        {
            const float clippedHeight = src->bottom - ty;
            screenBottom = (ty + clippedHeight - src->top) * heightRatio + dst->top;
            tvEnd = clippedHeight / texture->sh;
        }

        if (src->left > tx)
        {
            const float clippedLeft = src->left - tx;
            tuStart = clippedLeft / texture->sw;
            screenLeft = dst->left; // Align with destination left edge
        }

        if (src->top > ty)
        {
            const float clippedTop = src->top - ty;
            tvStart = clippedTop / texture->sh;
            screenTop = dst->top; // Align with destination top edge
        }

        visible.ScreenLeft = screenLeft;
        visible.ScreenTop = screenTop;
        visible.ScreenRight = screenRight;
        visible.ScreenBottom = screenBottom;
        visible.TexU = tuStart;
        visible.TexV = tvStart;
        visible.TexU2 = tuEnd;
        visible.TexV2 = tvEnd;
    }

    // Skip rendering if no visible textures
    if (visibleCount == 0)
    {
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        return TRUE;
    }

    // Sort visible textures by TextureId to minimize texture switches
    // Use insertion sort for small arrays (fast for nearly-sorted data)
    for (int i = 1; i < visibleCount; i++)
    {
        VisibleTexture key = visibleTextures[i];
        int j = i - 1;

        while (j >= 0 && visibleTextures[j].TextureId > key.TextureId)
        {
            visibleTextures[j + 1] = visibleTextures[j];
            j--;
        }

        visibleTextures[j + 1] = key;
    }

    // Allocate a vertex buffer for all quads at once
    const int totalVertices = visibleCount * 4;
    CKDWORD vertexBufferIndex = GetDynamicVertexBuffer(CKRST_VF_TLVERTEX, totalVertices, sizeof(CKVertex), 1);
    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[vertexBufferIndex]);
    if (!vb)
    {
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        return FALSE;
    }

    // Lock the vertex buffer
    void *pBuf = nullptr;
    CKDWORD startVertex = 0;

    if (vb->m_CurrentVCount + totalVertices <= vb->m_MaxVertexCount)
    {
        hr = vb->DxBuffer->Lock(sizeof(CKVertex) * vb->m_CurrentVCount, sizeof(CKVertex) * totalVertices, &pBuf,
                                D3DLOCK_NOOVERWRITE);
        startVertex = vb->m_CurrentVCount;
        vb->m_CurrentVCount += totalVertices;
    }
    else
    {
        hr = vb->DxBuffer->Lock(0, sizeof(CKVertex) * totalVertices, &pBuf, D3DLOCK_DISCARD);
        vb->m_CurrentVCount = totalVertices;
    }

    if (FAILED(hr) || !pBuf)
    {
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        return FALSE;
    }

    // Fill the vertex buffer
    CKVertex *vertices = static_cast<CKVertex *>(pBuf);
    const CKDWORD diffuseColor = (R_MASK | G_MASK | B_MASK | A_MASK);

    for (int i = 0; i < visibleCount; ++i)
    {
        const VisibleTexture &tex = visibleTextures[i];
        CKVertex *quad = &vertices[i * 4];

        // Set common vertex attributes
        for (int v = 0; v < 4; ++v)
        {
            quad[v].Diffuse = diffuseColor;
            quad[v].Specular = A_MASK;
        }

        // Top-left
        quad[0].V = VxVector4(tex.ScreenLeft, tex.ScreenTop, 0.0f, 1.0f);
        quad[0].tu = tex.TexU;
        quad[0].tv = tex.TexV;

        // Bottom-left
        quad[1].V = VxVector4(tex.ScreenLeft, tex.ScreenBottom, 0.0f, 1.0f);
        quad[1].tu = tex.TexU;
        quad[1].tv = tex.TexV2;

        // Bottom-right
        quad[2].V = VxVector4(tex.ScreenRight, tex.ScreenBottom, 0.0f, 1.0f);
        quad[2].tu = tex.TexU2;
        quad[2].tv = tex.TexV2;

        // Top-right
        quad[3].V = VxVector4(tex.ScreenRight, tex.ScreenTop, 0.0f, 1.0f);
        quad[3].tu = tex.TexU2;
        quad[3].tv = tex.TexV;
    }

    // Unlock the vertex buffer
    vb->DxBuffer->Unlock();

    // Setup vertex stream
    m_CurrentVertexBufferCache = NULL; // Force update
    SetupStreams(vb->DxBuffer, CKRST_VF_TLVERTEX, sizeof(CKVertex));

    // Render batches with the same texture
    CKDWORD currentTextureId = UINT_MAX;
    int batchStart = 0;

    for (int i = 0; i <= visibleCount; ++i)
    {
        // Process batch when texture changes or at the end
        if (i == visibleCount || visibleTextures[i].TextureId != currentTextureId)
        {
            // Render current batch if it exists
            if (i > batchStart)
            {
                // Draw all quads in the batch
                int quadsToDraw = i - batchStart;
                m_Device->DrawPrimitive(D3DPT_TRIANGLEFAN, startVertex + (batchStart * 4), quadsToDraw * 2);
            }

            // Setup new texture (if not at the end)
            if (i < visibleCount)
            {
                currentTextureId = visibleTextures[i].TextureId;
                CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[currentTextureId]);
                if (desc && desc->DxTexture)
                {
                    m_Device->SetTexture(0, desc->DxTexture);
                }
                else
                {
                    m_Device->SetTexture(0, NULL);
                }

                batchStart = i;
            }
        }
    }

    // Restore state
    m_Device->SetStreamSource(0, NULL, 0, 0);
    m_Device->SetTexture(0, NULL);
    m_Device->SetViewport(&oldViewport);

    // Restore render states
    SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
    SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
    SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
    SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);

    return TRUE;
}

void *CKDX9RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount, CKRST_LOCKFLAGS Lock)
{
    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return FALSE;

    void *pVertices = NULL;
    if (FAILED(vb->DxBuffer->Lock(StartVertex * vb->m_VertexSize, VertexCount * vb->m_VertexSize, &pVertices, Lock << 12)))
        return NULL;

    return pVertices;
}

CKBOOL CKDX9RasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return FALSE;

    return SUCCEEDED(vb->DxBuffer->Unlock());
}

int CKDX9RasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    D3DSURFACE_DESC desc = {};
    IDirect3DSurface9 *surface = NULL;
    int depthBytesPerPixel = 0;
    HRESULT hr;

    // Step 1: Get the appropriate surface based on buffer type
    switch (buffer)
    {
        case VXBUFFER_BACKBUFFER:
            {
                hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
                if (FAILED(hr) || !surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                if (FAILED(hr))
                {
                    SAFERELEASE(surface);
                    return 0;
                }

                VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(desc.Format);
                VxPixelFormat2ImageDesc(vxpf, img_desc);
                break;
            }
        case VXBUFFER_ZBUFFER:
            {
                hr = m_Device->GetDepthStencilSurface(&surface);
                if (FAILED(hr) || !surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                if (FAILED(hr))
                {
                    SAFERELEASE(surface);
                    return 0;
                }

                img_desc.BitsPerPixel = 32;
                img_desc.AlphaMask = 0;
                img_desc.BlueMask = B_MASK;
                img_desc.GreenMask = G_MASK;
                img_desc.RedMask = R_MASK;

                switch (desc.Format)
                {
                    case D3DFMT_D16_LOCKABLE:
                    case D3DFMT_D15S1:
                    case D3DFMT_D16:
                        depthBytesPerPixel = 2;
                        break;
                    case D3DFMT_D32:
                    case D3DFMT_D24S8:
                    case D3DFMT_D24X8:
                    case D3DFMT_D24X4S4:
                        depthBytesPerPixel = 4;
                        break;
                    default:
                        depthBytesPerPixel = 0;
                        break;
                }
                break; // Add missing break to prevent fallthrough
            }
        case VXBUFFER_STENCILBUFFER:
            {
                hr = m_Device->GetDepthStencilSurface(&surface);
                if (FAILED(hr) || !surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                if (FAILED(hr))
                {
                    SAFERELEASE(surface);
                    return 0;
                }

                D3DFORMAT d3dFormat = desc.Format;
                img_desc.BitsPerPixel = 32;
                img_desc.AlphaMask = 0;
                img_desc.BlueMask = B_MASK;
                img_desc.GreenMask = G_MASK;
                img_desc.RedMask = R_MASK;

                if (d3dFormat == D3DFMT_D15S1)
                {
                    depthBytesPerPixel = 2;
                }
                else if (d3dFormat == D3DFMT_D24S8 || d3dFormat == D3DFMT_D24X4S4)
                {
                    depthBytesPerPixel = 4;
                }
                else
                {
                    // Unsupported stencil format
                    SAFERELEASE(surface);
                    return 0;
                }
                break;
            }
        default:
            return 0;
    }

    // Step 2: Calculate the boundaries of the region to copy
    UINT right, left, top, bottom;
    if (rect)
    {
        // Clamp rectangle to surface boundaries
        right = (rect->right > (int)desc.Width) ? desc.Width : rect->right;
        left = (rect->left < 0) ? 0 : rect->left;
        top = (rect->top < 0) ? 0 : rect->top;
        bottom = (rect->bottom > (int)desc.Height) ? desc.Height : rect->bottom;
    }
    else
    {
        // Use full surface if no rectangle specified
        top = 0;
        bottom = desc.Height;
        left = 0;
        right = desc.Width;
    }

    // Validate rectangle dimensions
    if (left >= right || top >= bottom)
    {
        SAFERELEASE(surface);
        return 0;
    }

    // Step 3: Set up the image description
    UINT width = right - left;
    UINT height = bottom - top;
    img_desc.Width = width;
    img_desc.Height = height;
    int bytesPerPixel = img_desc.BitsPerPixel / 8;
    img_desc.BytesPerLine = width * bytesPerPixel;

    // Validate image parameters
    if (img_desc.BytesPerLine == 0 || !img_desc.Image)
    {
        SAFERELEASE(surface);
        return 0;
    }

    // Step 4: Copy the data
    int totalBytes = 0;
    IDirect3DSurface9 *tempSurface = NULL;
    RECT srcRect = {(LONG)left, (LONG)top, (LONG)right, (LONG)bottom};

    // Try to create a temporary surface for the copy operation
    hr = m_Device->CreateOffscreenPlainSurface(width, height, desc.Format, D3DPOOL_SCRATCH, &tempSurface, NULL);
    if (SUCCEEDED(hr) && tempSurface)
    {
        // Copy source region to temp surface
        POINT destPoint = {0, 0};
        hr = m_Device->UpdateSurface(surface, &srcRect, tempSurface, &destPoint);
        if (SUCCEEDED(hr))
        {
            // Lock the entire temp surface
            D3DLOCKED_RECT lockedRect;
            hr = tempSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY);
            if (SUCCEEDED(hr))
            {
                BYTE *srcData = (BYTE *)lockedRect.pBits;
                BYTE *destData = img_desc.Image;

                if (depthBytesPerPixel == 2 && bytesPerPixel == 4)
                {
                    // Convert 16-bit depth/stencil to 32-bit RGB
                    for (UINT row = 0; row < height; ++row)
                    {
                        BYTE *srcRow = srcData + (row * lockedRect.Pitch);
                        BYTE *destRow = destData + (row * img_desc.BytesPerLine);

                        for (UINT col = 0; col < width; ++col)
                        {
                            // Read 16-bit depth value
                            WORD depthValue = *((WORD *)(srcRow + col * 2));

                            // Write as 32-bit RGB value (scale to visible range)
                            DWORD *destPixel = (DWORD *)(destRow + col * 4);
                            BYTE intensity = (BYTE)((depthValue * 255) / 65535);
                            *destPixel = (intensity << 16) | (intensity << 8) | intensity;
                        }
                    }
                }
                else
                {
                    // Standard copy for normal color buffers or 32-bit depth/stencil
                    for (UINT row = 0; row < height; ++row)
                    {
                        memcpy(destData + (row * img_desc.BytesPerLine), srcData + (row * lockedRect.Pitch), width * bytesPerPixel);
                    }
                }

                tempSurface->UnlockRect();
                totalBytes = img_desc.BytesPerLine * height;
            }
        }

        SAFERELEASE(tempSurface);
    }
    else
    {
        // Fallback: try to lock the original surface directly
        D3DLOCKED_RECT lockedRect;
        hr = surface->LockRect(&lockedRect, &srcRect, D3DLOCK_READONLY);
        if (SUCCEEDED(hr))
        {
            BYTE *srcData = (BYTE *)lockedRect.pBits;
            BYTE *destData = img_desc.Image;

            if (depthBytesPerPixel == 2 && bytesPerPixel == 4)
            {
                // Convert 16-bit depth/stencil to 32-bit RGB
                for (UINT row = 0; row < height; ++row)
                {
                    BYTE *srcRow = srcData + (row * lockedRect.Pitch);
                    BYTE *destRow = destData + (row * img_desc.BytesPerLine);

                    for (UINT col = 0; col < width; ++col)
                    {
                        // Read 16-bit depth value
                        WORD depthValue = *((WORD *)(srcRow + col * 2));

                        // Write as 32-bit RGB value (scale to visible range)
                        DWORD *destPixel = (DWORD *)(destRow + col * 4);
                        BYTE intensity = (BYTE)((depthValue * 255) / 65535);
                        *destPixel = (intensity << 16) | (intensity << 8) | intensity;
                    }
                }
            }
            else
            {
                // Standard copy for normal color buffers or 32-bit depth/stencil
                for (UINT row = 0; row < height; ++row)
                {
                    memcpy(destData + (row * img_desc.BytesPerLine), srcData + (row * lockedRect.Pitch), width * bytesPerPixel);
                }
            }

            surface->UnlockRect();
            totalBytes = img_desc.BytesPerLine * height;
        }
    }

    // Cleanup
    SAFERELEASE(surface);
    return totalBytes;
}

int CKDX9RasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    // Input validation
    if (!img_desc.Image)
        return 0;

    // Only backbuffer copying is supported
    if (buffer != VXBUFFER_BACKBUFFER)
        return 0;

    // Get back buffer
    IDirect3DSurface9 *backBuffer = NULL;
    HRESULT hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (FAILED(hr) || !backBuffer)
        return 0;

    // Get back buffer description
    D3DSURFACE_DESC desc = {};
    hr = backBuffer->GetDesc(&desc);
    if (FAILED(hr))
    {
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Get back buffer format information
    VxImageDescEx backBufferDesc;
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(desc.Format);
    VxPixelFormat2ImageDesc(vxpf, backBufferDesc);

    // Calculate destination rectangle
    int left = 0, top = 0, right = 0, bottom = 0;
    if (rect)
    {
        // Use provided rectangle, clamped to backbuffer bounds
        left = max(0, rect->left);
        top = max(0, rect->top);
        right = min((int)desc.Width, rect->right);
        bottom = min((int)desc.Height, rect->bottom);
    }
    else
    {
        // Use full backbuffer size
        right = desc.Width;
        bottom = desc.Height;
    }

    // Validate resulting rectangle
    int destWidth = right - left;
    int destHeight = bottom - top;
    if (destWidth <= 0 || destHeight <= 0)
    {
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Verify format compatibility
    int bytesPerPixel = img_desc.BitsPerPixel / 8;
    if (img_desc.BitsPerPixel != backBufferDesc.BitsPerPixel)
    {
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Create temporary surface matching source image size
    IDirect3DSurface9 *tempSurface = NULL;
    hr = m_Device->CreateOffscreenPlainSurface(
        img_desc.Width, 
        img_desc.Height, 
        desc.Format, 
        D3DPOOL_SCRATCH,
        &tempSurface, 
        NULL
    );
    if (FAILED(hr) || !tempSurface)
    {
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Lock the temporary surface for writing
    D3DLOCKED_RECT lockedRect;
    hr = tempSurface->LockRect(&lockedRect, NULL, 0);
    if (FAILED(hr))
    {
        SAFERELEASE(tempSurface);
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Copy the image data to the temporary surface
    BYTE *destData = static_cast<BYTE *>(lockedRect.pBits);
    BYTE *srcData = img_desc.Image;

    // Calculate proper row size (no partial copy needed)
    UINT rowSize = min(img_desc.BytesPerLine, (UINT)lockedRect.Pitch);

    // Copy each row
    for (UINT y = 0; y < img_desc.Height; ++y)
    {
        memcpy(destData, srcData, rowSize);
        destData += lockedRect.Pitch;
        srcData += img_desc.BytesPerLine;
    }

    // Unlock the surface
    hr = tempSurface->UnlockRect();
    if (FAILED(hr))
    {
        SAFERELEASE(tempSurface);
        SAFERELEASE(backBuffer);
        return 0;
    }

    // Define source and destination rectangles for copying to back buffer
    RECT srcRect = {0, 0, min(img_desc.Width, (UINT)destWidth), min(img_desc.Height, (UINT)destHeight)};
    POINT destPoint = {left, top};

    // Copy from temporary surface to back buffer
    // Note: UpdateSurface requires: source surface, source rect, destination surface, destination point
    hr = m_Device->UpdateSurface(tempSurface, &srcRect, backBuffer, &destPoint);

    // Clean up
    SAFERELEASE(tempSurface);
    SAFERELEASE(backBuffer);

    // Return bytes processed if successful, otherwise 0
    if (SUCCEEDED(hr))
    {
        return img_desc.BytesPerLine * img_desc.Height;
    }
    else
    {
        return 0;
    }
}

void CKDX9RasterizerContext::SetTransparentMode(CKBOOL Trans)
{
    m_TransparentMode = Trans;
    if (!Trans)
        ReleaseScreenBackup();
}

CKBOOL CKDX9RasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)
{
    return SUCCEEDED(m_Device->SetClipPlane(ClipPlaneIndex, (const float *)&PlaneEquation));
}

CKBOOL CKDX9RasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)
{
    return SUCCEEDED(m_Device->GetClipPlane(ClipPlaneIndex, (float *)&PlaneEquation));
}

CKBOOL CKDX9RasterizerContext::LoadCubeMapTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, CKRST_CUBEFACE Face, int miplevel)
{
    if (Texture >= m_Textures.Size())
        return FALSE;

    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxCubeTexture)
        return FALSE;

    if ((desc->Flags & CKRST_TEXTURE_RENDERTARGET) != 0)
        return TRUE;

    if ((desc->Flags & CKRST_TEXTURE_CUBEMAP) == 0)
        return FALSE;

    HRESULT hr;

    int actualMipLevel = (miplevel < 0) ? 0 : miplevel;
    D3DSURFACE_DESC surfaceDesc;
    hr = desc->DxCubeTexture->GetLevelDesc(actualMipLevel, &surfaceDesc);
    assert(SUCCEEDED(hr));

    IDirect3DSurface9 *pSurface = NULL;

    if ((surfaceDesc.Format == D3DFMT_DXT1 ||
        surfaceDesc.Format == D3DFMT_DXT2 ||
        surfaceDesc.Format == D3DFMT_DXT3 ||
        surfaceDesc.Format == D3DFMT_DXT4 ||
        surfaceDesc.Format == D3DFMT_DXT5) &&
        (D3DXLoadSurfaceFromSurface && D3DXLoadSurfaceFromMemory))
    {
        IDirect3DSurface9 *pCubeMapSurface = NULL;
        desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, actualMipLevel, &pCubeMapSurface);
        if (!pCubeMapSurface)
            return FALSE;

        RECT srcRect{0, 0, SurfDesc.Height, SurfDesc.Width};
        VX_PIXELFORMAT vxpf = VxImageDesc2PixelFormat(SurfDesc);
        D3DFORMAT format = VxPixelFormatToD3DFormat(vxpf);
        hr = D3DXLoadSurfaceFromMemory(pCubeMapSurface, NULL, NULL, SurfDesc.Image, format, SurfDesc.BytesPerLine, NULL, &srcRect, D3DX_FILTER_LINEAR, 0);
        assert(SUCCEEDED(hr));

        CKDWORD mipMapCount = m_Textures[Texture]->MipMapCount;
        if (miplevel == -1 && mipMapCount > 0)
        {
            for (int i = 1; i < mipMapCount + 1; ++i)
            {
                desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, i, &pSurface);
                hr = D3DXLoadSurfaceFromSurface(pSurface, NULL, NULL, pCubeMapSurface, NULL, NULL, D3DX_FILTER_BOX, 0);
                assert(SUCCEEDED(hr));
                SAFERELEASE(pCubeMapSurface);
            }
        }
        else
        {
            pSurface = pCubeMapSurface;
        }

        SAFERELEASE(pSurface);
        return SUCCEEDED(hr);
    }

    VxImageDescEx src = SurfDesc;
    VxImageDescEx dst;
    if (miplevel != -1 || desc->MipMapCount == 0)
        pSurface = NULL;
    CKBYTE *image = NULL;

    if (pSurface)
    {
        image = m_Owner->AllocateObjects(surfaceDesc.Width * surfaceDesc.Height);
        if (surfaceDesc.Width != src.Width || surfaceDesc.Height != src.Height)
        {
            dst.Width = src.Width;
            dst.Height = src.Height;
            dst.BitsPerPixel = 32;
            dst.BytesPerLine = 4 * surfaceDesc.Width;
            dst.AlphaMask = A_MASK;
            dst.RedMask = R_MASK;
            dst.GreenMask = G_MASK;
            dst.BlueMask = B_MASK;
            dst.Image = image;
            VxDoBlit(src, dst);
            src = dst;
        }
    }

    D3DLOCKED_RECT lockRect;
    hr = desc->DxCubeTexture->LockRect((D3DCUBEMAP_FACES)Face, actualMipLevel, &lockRect, NULL, 0);
    if (FAILED(hr))
        return FALSE;

    LoadSurface(surfaceDesc, lockRect, src);

    hr = desc->DxCubeTexture->UnlockRect((D3DCUBEMAP_FACES)Face, actualMipLevel);
    assert(SUCCEEDED(hr));

    if (pSurface)
    {
        dst = src;
        for (int i = 1; i < desc->MipMapCount + 1; ++i)
        {
            VxGenerateMipMap(dst, image);

            if (dst.Width > 1)
                dst.Width >>= 1;
            if (dst.Height > 1)
                dst.Height >>= 1;
            dst.BytesPerLine = 4 * dst.Width;
            dst.Image = image;

            desc->DxCubeTexture->GetLevelDesc(i, &surfaceDesc);

            hr = desc->DxCubeTexture->LockRect((D3DCUBEMAP_FACES)Face, i, &lockRect, NULL, NULL);
            if (FAILED(hr))
            {
                SAFERELEASE(pSurface);
                return FALSE;
            }

            LoadSurface(surfaceDesc, lockRect, dst);

            hr = desc->DxCubeTexture->UnlockRect((D3DCUBEMAP_FACES)Face, i);
            assert(SUCCEEDED(hr));

            if (dst.Width <= 1)
                return TRUE;
        }
    }

    return TRUE;
}

void *CKDX9RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    CKDX9IndexBufferDesc *ib = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ib || !ib->DxBuffer)
        return FALSE;

    void *pIndices = NULL;
    if (FAILED(ib->DxBuffer->Lock(StartIndex * 2, IndexCount * 2, &pIndices, Lock << 12)))
        return NULL;

    return pIndices;
}

CKBOOL CKDX9RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    CKDX9IndexBufferDesc *ib = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ib || !ib->DxBuffer)
        return FALSE;

    return SUCCEEDED(ib->DxBuffer->Unlock());
}

CKBOOL CKDX9RasterizerContext::CreateTextureFromFile(CKDWORD Texture, const char *Filename, TexFromFile *param)
{
    if (!Filename)
        return FALSE;

    if (!D3DXCreateTextureFromFileExA)
        return FALSE;

    D3DFORMAT format = VxPixelFormatToD3DFormat(param->Format);
    if (format == D3DFMT_UNKNOWN)
        format = D3DFMT_A8R8G8B8;

    int mipLevel = -1;
    if (param->MipLevels == 0)
        mipLevel = 1;

    IDirect3DTexture9 *pTexture = NULL;
    D3DXCreateTextureFromFileExA(m_Device, Filename, D3DX_DEFAULT, D3DX_DEFAULT, mipLevel, 0, format, D3DPOOL_DEFAULT,
                                 D3DX_DEFAULT, D3DX_DEFAULT, param->ColorKey, NULL, NULL, &pTexture);
    if (!pTexture)
        return FALSE;

    HRESULT hr;
    D3DSURFACE_DESC surfaceDesc = {};
    hr = pTexture->GetLevelDesc(0, &surfaceDesc);
    assert(SUCCEEDED(hr));

    CKDX9TextureDesc *desc = (CKDX9TextureDesc *)m_Textures[Texture];
    if (desc)
        delete desc;

    desc = new CKDX9TextureDesc;
    desc->DxTexture = pTexture;
    D3DFormatToTextureDesc(surfaceDesc.Format, desc);
    desc->Flags = CKRST_TEXTURE_VALID | CKRST_TEXTURE_RGB;
    desc->Format.Width = surfaceDesc.Width;
    desc->Format.Height = surfaceDesc.Height;
    desc->MipMapCount = pTexture->GetLevelCount() - 1;
    m_Textures[Texture] = desc;

    return TRUE;
}

void CKDX9RasterizerContext::UpdateDirectXData()
{
    IDirect3DSurface9 *pBackBuffer = NULL;
    HRESULT hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    assert(SUCCEEDED(hr));

    IDirect3DSurface9 *pZStencilSurface = NULL;
    hr = m_Device->GetDepthStencilSurface(&pZStencilSurface);
    assert(SUCCEEDED(hr));

    m_DirectXData.D3DDevice = m_Device;
    m_DirectXData.DxVersion = DIRECT3D_VERSION;
    m_DirectXData.D3DViewport = NULL;
    m_DirectXData.DDBackBuffer = pBackBuffer;
    m_DirectXData.DDPrimaryBuffer = NULL;
    m_DirectXData.DDZBuffer = pZStencilSurface;
    m_DirectXData.DirectDraw = NULL;
    m_DirectXData.Direct3D = m_Owner->m_D3D9;
    m_DirectXData.DDClipper = NULL;

    SAFERELEASE(pZStencilSurface);
    SAFERELEASE(pBackBuffer);
}

CKBOOL CKDX9RasterizerContext::InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc *VB,
                                                       CKDWORD StartIndex, CKDWORD VertexCount, WORD *indices,
                                                       int indexcount, CKBOOL Clip)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    HRESULT hr;
    int ibstart = 0;

    if (indices)
    {
        CKDX9IndexBufferDesc *desc = m_IndexBuffer[Clip];
        if (!desc || indexcount > desc->m_MaxIndexCount || !desc->DxBuffer)
        {
            if (desc)
            {
                delete desc;
                desc = NULL;
            }

            desc = new CKDX9IndexBufferDesc;

            int maxIndexCount = indexcount + 100;
            if (maxIndexCount <= 10000)
                maxIndexCount = 10000;

            DWORD usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
            if (m_SoftwareVertexProcessing)
                usage |= D3DUSAGE_SOFTWAREPROCESSING;

            hr = m_Device->CreateIndexBuffer(2 * maxIndexCount, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &desc->DxBuffer, NULL);
            assert(SUCCEEDED(hr));

            desc->m_MaxIndexCount = maxIndexCount;
            m_IndexBuffer[Clip] = desc;
        }

        void *pbData = NULL;
        if (indexcount + desc->m_CurrentICount <= desc->m_MaxIndexCount)
        {
#ifdef TRACY_ENABLE
            ZoneScopedN("Lock");
#endif
            hr = desc->DxBuffer->Lock(2 * desc->m_CurrentICount, 2 * indexcount, &pbData, D3DLOCK_NOOVERWRITE);
            assert(SUCCEEDED(hr));
            ibstart = desc->m_CurrentICount;
            desc->m_CurrentICount += indexcount;
        }
        else
        {
#ifdef TRACY_ENABLE
            ZoneScopedN("Lock");
#endif
            hr = desc->DxBuffer->Lock(0, 2 * indexcount, &pbData, D3DLOCK_DISCARD);
            assert(SUCCEEDED(hr));
            desc->m_CurrentICount = indexcount;
        }
        if (pbData)
        {
            memcpy(pbData, indices, 2 * indexcount);
        }

        hr = desc->DxBuffer->Unlock();
        assert(SUCCEEDED(hr));
    }

    SetupStreams(VB->DxBuffer, VB->m_VertexFormat, VB->m_VertexSize);

    int primitiveCount = (indexcount == 0) ? VertexCount : indexcount;

    switch (pType)
    {
        case VX_LINELIST:
            primitiveCount /= 2;
            break;
        case VX_LINESTRIP:
            primitiveCount -= 1;
            break;
        case VX_TRIANGLELIST:
            primitiveCount /= 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            primitiveCount -= 2;
            break;
        default:
            break;
    }

    if (!indices || pType == VX_POINTLIST)
    {
        hr = m_Device->DrawPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, primitiveCount);
        return SUCCEEDED(hr);
    }

    hr = m_Device->SetIndices(m_IndexBuffer[Clip]->DxBuffer);
    assert(SUCCEEDED(hr));

    return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, 0, VertexCount, ibstart, primitiveCount));
}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    HRESULT hr;

    CKBOOL fixed = FALSE;

    if (m_CurrentVertexShaderCache != 0 && m_CurrentVertexShaderCache < m_VertexShaders.Size())
    {
        CKDX9VertexShaderDesc *desc = (CKDX9VertexShaderDesc *)m_VertexShaders[m_CurrentVertexShaderCache];
        if (desc->DxShader)
        {
            IDirect3DVertexDeclaration9 *pDecl = NULL;

            auto it = m_VertexDeclarations.Find(VFormat);
            if (it != m_VertexDeclarations.End())
            {
                pDecl = *it;
            }
            else
            {
                if (CreateVertexDeclaration(VFormat, &pDecl))
                {
                    m_VertexDeclarations.Insert(VFormat, pDecl);
                }
                else
                {
                    fixed = TRUE;
                }
            }

            if (!fixed)
            {
                hr = m_Device->SetVertexDeclaration(pDecl);
                assert(SUCCEEDED(hr));

                hr = m_Device->SetVertexShader(desc->DxShader);
                assert(SUCCEEDED(hr));
            }
        }
        else
        {
            fixed = TRUE;
        }
    }
    else
    {
        fixed = TRUE;
    }

    if (fixed)
    {
        m_CurrentVertexShaderCache = 0;
        if (VFormat != m_CurrentVertexFormatCache)
            m_CurrentVertexFormatCache = VFormat;

        hr = m_Device->SetVertexShader(NULL);
        assert(SUCCEEDED(hr));

        hr = m_Device->SetFVF(m_CurrentVertexFormatCache);
        assert(SUCCEEDED(hr));
    }

    if (Buffer != m_CurrentVertexBufferCache || m_CurrentVertexSizeCache != VSize)
    {
        hr = m_Device->SetStreamSource(0, Buffer, 0, VSize);
        assert(SUCCEEDED(hr));
        m_CurrentVertexBufferCache = Buffer;
        m_CurrentVertexSizeCache = VSize;
    }
}

CKBOOL CKDX9RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
    if (!DesiredFormat)
        return FALSE;

    if (DesiredFormat->MipMapCount == 1)
        DesiredFormat->MipMapCount = 0;

    CKDX9RasterizerDriver *driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);

    D3DFORMAT format = driver->FindNearestTextureFormat(DesiredFormat, m_PresentParams.BackBufferFormat, D3DUSAGE_DYNAMIC);
    if (format == D3DFMT_UNKNOWN)
        format = D3DFMT_A8R8G8B8;

    int width = DesiredFormat->Format.Width;
    int height = DesiredFormat->Format.Height;

    CKDWORD flags = DesiredFormat->Flags;
    CKDWORD textureCaps = driver->m_3DCaps.TextureCaps;
    if ((textureCaps & CKRST_TEXTURECAPS_POW2) != 0)
    {
        int n;
        for (n = 1; n < width; n *= 2)
            continue;
        width = n;

        for (n = 1; n < height; n *= 2)
            continue;
        height = n;
    }

    if (((flags & CKRST_TEXTURE_CUBEMAP) != 0 || (textureCaps & CKRST_TEXTURECAPS_SQUAREONLY) != 0) && width != height)
    {
        if (width <= height)
            width = height;
        else
            height = width;
    }

    if (width < driver->m_3DCaps.MinTextureWidth)
        width = driver->m_3DCaps.MinTextureWidth;
    if (width > driver->m_3DCaps.MaxTextureWidth)
        width = driver->m_3DCaps.MaxTextureWidth;
    if (height < driver->m_3DCaps.MinTextureHeight)
        height = driver->m_3DCaps.MinTextureHeight;
    if (height > driver->m_3DCaps.MaxTextureHeight)
        height = driver->m_3DCaps.MaxTextureHeight;

    UINT levels = DesiredFormat->MipMapCount != 0 ? DesiredFormat->MipMapCount : 1;
    D3DPOOL pool = (flags & CKRST_TEXTURE_MANAGED) != 0 ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;

    if ((flags & CKRST_TEXTURE_CUBEMAP) == 0)
    {
        IDirect3DTexture9 *pTexture = NULL;
        if (FAILED(m_Device->CreateTexture(width, height, levels, 0, format, pool, &pTexture, NULL)))
            return FALSE;

        D3DSURFACE_DESC surfaceDesc = {};
        pTexture->GetLevelDesc(0, &surfaceDesc);

        CKDX9TextureDesc *desc = (CKDX9TextureDesc *)m_Textures[Texture];
        if (desc)
            delete desc;
        desc = new CKDX9TextureDesc;
        desc->DxTexture = pTexture;
        D3DFormatToTextureDesc(surfaceDesc.Format, desc);
        desc->Flags = DesiredFormat->Flags;
        desc->Format.Width = surfaceDesc.Width;
        desc->Format.Height = surfaceDesc.Height;
        desc->MipMapCount = pTexture->GetLevelCount() - 1;
        m_Textures[Texture] = desc;
    }
    else
    {
        IDirect3DCubeTexture9 *pCubeTexture = NULL;
        if (FAILED(m_Device->CreateCubeTexture(width, levels, 0, format, pool, &pCubeTexture, NULL)))
            return FALSE;

        D3DSURFACE_DESC surfaceDesc = {};
        pCubeTexture->GetLevelDesc(0, &surfaceDesc);

        CKDX9TextureDesc *desc = (CKDX9TextureDesc *)m_Textures[Texture];
        if (desc)
            delete desc;
        desc = new CKDX9TextureDesc;
        desc->DxCubeTexture = pCubeTexture;
        D3DFormatToTextureDesc(surfaceDesc.Format, desc);
        desc->Flags = DesiredFormat->Flags;
        desc->Format.Width = surfaceDesc.Width;
        desc->Format.Height = surfaceDesc.Height;
        desc->MipMapCount = pCubeTexture->GetLevelCount() - 1;
        m_Textures[Texture] = desc;
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
#if LOGGING && LOG_CREATEVERTEXSHADER
    fprintf(stderr, "create vertex shader %d\n", VShader);
#endif
    if (VShader >= m_VertexShaders.Size() || !DesiredFormat)
        return FALSE;

    CKVertexShaderDesc *shader = m_VertexShaders[VShader];
    if (DesiredFormat == shader)
    {
        CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat);
    }

    if (shader)
    {
        delete shader;
        m_VertexShaders[VShader] = NULL;
    }

    CKDX9VertexShaderDesc *desc = new CKDX9VertexShaderDesc;
    if (!desc)
        return FALSE;

    m_VertexShaders[VShader] = desc;
    if (!desc->Create(this, DesiredFormat))
        return FALSE;

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{
#if LOGGING && LOG_CREATEPIXELSHADER
    fprintf(stderr, "create pixel shader %d\n", PShader);
#endif
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return FALSE;

    CKPixelShaderDesc *shader = m_PixelShaders[PShader];
    if (DesiredFormat == shader)
    {
        CKDX9PixelShaderDesc *desc = static_cast<CKDX9PixelShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat);
    }

    if (shader)
    {
        delete shader;
        m_PixelShaders[PShader] = NULL;
    }

    CKDX9PixelShaderDesc *desc = new CKDX9PixelShaderDesc;
    if (!desc)
        return FALSE;

    m_PixelShaders[PShader] = desc;
    if (!desc->Create(this, DesiredFormat))
        return FALSE;

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return FALSE;

    DWORD fvf = DesiredFormat->m_VertexFormat;
    DWORD size = DesiredFormat->m_VertexSize;
    DWORD usage = 0;
    if (DesiredFormat->m_Flags & CKRST_VB_DYNAMIC)
        usage |= D3DUSAGE_DYNAMIC;
    if (DesiredFormat->m_Flags & CKRST_VB_WRITEONLY)
        usage |= D3DUSAGE_WRITEONLY;

    IDirect3DVertexBuffer9 *buffer = NULL;
    if (FAILED(m_Device->CreateVertexBuffer(DesiredFormat->m_MaxVertexCount * size, usage, fvf, D3DPOOL_DEFAULT, &buffer, NULL)))
        return FALSE;

    if (m_VertexBuffers[VB] == DesiredFormat)
    {
        CKDX9VertexBufferDesc *desc = static_cast<CKDX9VertexBufferDesc *>(DesiredFormat);
        desc->DxBuffer = buffer;
        desc->m_Flags |= CKRST_VB_VALID;
        return TRUE;
    }

    if (m_VertexBuffers[VB])
        delete m_VertexBuffers[VB];

    CKDX9VertexBufferDesc *desc = new CKDX9VertexBufferDesc;
    if (!desc)
        return FALSE;
    desc->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    desc->m_VertexSize = size;
    desc->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount;
    desc->m_VertexFormat = fvf;
    desc->m_Flags = DesiredFormat->m_Flags;
    desc->DxBuffer = buffer;
    desc->m_Flags |= CKRST_VB_VALID;
    m_VertexBuffers[VB] = desc;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;

    DWORD usage = D3DUSAGE_WRITEONLY;
    if ((DesiredFormat->m_Flags & CKRST_VB_DYNAMIC) != 0)
        usage |= D3DUSAGE_DYNAMIC;

    IDirect3DIndexBuffer9 *buffer;
    if (FAILED(m_Device->CreateIndexBuffer(2 * DesiredFormat->m_MaxIndexCount, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &buffer, NULL)))
        return FALSE;

    if (DesiredFormat == m_IndexBuffers[IB])
    {
        CKDX9IndexBufferDesc *desc = static_cast<CKDX9IndexBufferDesc *>(DesiredFormat);
        desc->DxBuffer = buffer;
        desc->m_Flags |= CKRST_VB_VALID;
        return TRUE;
    }

    if (m_IndexBuffers[IB])
        delete m_IndexBuffers[IB];

    CKDX9IndexBufferDesc *desc = new CKDX9IndexBufferDesc;
    if (!desc)
        return FALSE;
    desc->m_CurrentICount = DesiredFormat->m_CurrentICount;
    desc->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount;
    desc->m_Flags = DesiredFormat->m_Flags;
    desc->DxBuffer = buffer;
    desc->m_Flags |= CKRST_VB_VALID;
    m_IndexBuffers[IB] = desc;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreateVertexDeclaration(CKDWORD VFormat, LPDIRECT3DVERTEXDECLARATION9 *ppDecl)
{
    if (!D3DXDeclaratorFromFVF)
        return FALSE;

    HRESULT hr;

    D3DVERTEXELEMENT9 declarator[MAX_FVF_DECL_SIZE];
    hr = D3DXDeclaratorFromFVF(VFormat, declarator);
    if (FAILED(hr))
        return FALSE;

    IDirect3DVertexDeclaration9 *pDecl = NULL;
    hr = m_Device->CreateVertexDeclaration(declarator, &pDecl);
    if (FAILED(hr))
        return FALSE;

    *ppDecl = pDecl;
    return TRUE;
}

void CKDX9RasterizerContext::FlushCaches()
{
    FlushRenderStateCache();

    m_InverseWinding = FALSE;

    memset(m_TextureMinFilterStateBlocks, NULL, sizeof(m_TextureMinFilterStateBlocks));
    memset(m_TextureMagFilterStateBlocks, NULL, sizeof(m_TextureMagFilterStateBlocks));
    memset(m_TextureMapBlendStateBlocks, NULL, sizeof(m_TextureMapBlendStateBlocks));

#if USE_D3DSTATEBLOCKS
    if (m_Device)
    {
        HRESULT hr;
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                hr = m_Device->BeginStateBlock();
                assert(SUCCEEDED(hr));
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMinFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                SetTextureStageState(i, CKRST_TSS_MINFILTER, j);
                hr = m_Device->EndStateBlock(&m_TextureMinFilterStateBlocks[j][i]);
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMinFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                assert(SUCCEEDED(hr));

                hr = m_Device->BeginStateBlock();
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMagFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                assert(SUCCEEDED(hr));
                SetTextureStageState(i, CKRST_TSS_MAGFILTER, j);
                hr = m_Device->EndStateBlock(&m_TextureMagFilterStateBlocks[j][i]);
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMagFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                assert(SUCCEEDED(hr));
            }
            for (int k = 0; k < 10; k++)
            {
                hr = m_Device->BeginStateBlock();
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMapBlendStateBlocks %d %d -> 0x%x\n", i, k, hr);
#endif
                assert(SUCCEEDED(hr));
                SetTextureStageState(i, CKRST_TSS_TEXTUREMAPBLEND, k);
                hr = m_Device->EndStateBlock(&m_TextureMapBlendStateBlocks[k][i]);
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMapBlendStateBlocks %d %d -> 0x%x\n", i, k, hr);
#endif
                assert(SUCCEEDED(hr));
            }
        }
    }
#endif
}

void CKDX9RasterizerContext::FlushNonManagedObjects()
{
    HRESULT hr;
    if (m_Device)
    {
        hr = m_Device->SetIndices(NULL);
        assert(SUCCEEDED(hr));

        hr = m_Device->SetStreamSource(0, NULL, NULL, NULL);
        assert(SUCCEEDED(hr));

        if (m_DefaultBackBuffer && m_DefaultDepthBuffer)
        {
            hr = m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
            assert(SUCCEEDED(hr));

            hr = m_Device->SetDepthStencilSurface(m_DefaultDepthBuffer);
            assert(SUCCEEDED(hr));

            SAFERELEASE(m_DefaultBackBuffer);
            SAFERELEASE(m_DefaultDepthBuffer);
        }
    }

    m_CurrentTextureIndex = 0;
    for (auto it = m_Textures.Begin(); it != m_Textures.End(); ++it)
    {
        CKTextureDesc *desc = *it;
        if (desc && (desc->Flags & CKRST_TEXTURE_MANAGED) == 0)
        {
            delete desc;
            *it = NULL;
        }
    }

    ReleaseTempZBuffers();
    FlushObjects(CKRST_OBJ_VERTEXBUFFER | CKRST_OBJ_INDEXBUFFER | CKRST_OBJ_VERTEXSHADER | CKRST_OBJ_PIXELSHADER);
    ReleaseIndexBuffers();
}

void CKDX9RasterizerContext::ReleaseStateBlocks()
{
    if (m_Device)
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                SAFERELEASE(m_TextureMinFilterStateBlocks[j][i]);
                SAFERELEASE(m_TextureMagFilterStateBlocks[j][i]);
            }

            for (int k = 0; k < 10; k++)
            {
                SAFERELEASE(m_TextureMapBlendStateBlocks[k][i]);
            }
        }

        memset(m_TextureMinFilterStateBlocks, 0, sizeof(m_TextureMinFilterStateBlocks));
        memset(m_TextureMagFilterStateBlocks, 0, sizeof(m_TextureMagFilterStateBlocks));
        memset(m_TextureMapBlendStateBlocks, 0, sizeof(m_TextureMapBlendStateBlocks));
    }
}

void CKDX9RasterizerContext::ReleaseIndexBuffers()
{
    if (m_IndexBuffer[0])
        delete m_IndexBuffer[0];
    if (m_IndexBuffer[1])
        delete m_IndexBuffer[1];
    m_IndexBuffer[0] = NULL;
    m_IndexBuffer[1] = NULL;
}

void CKDX9RasterizerContext::ClearStreamCache()
{
    m_CurrentVertexBufferCache = NULL;
    m_CurrentVertexSizeCache = 0;
    m_CurrentVertexFormatCache = 0;
    m_CurrentVertexShaderCache = 0;
}

void CKDX9RasterizerContext::ReleaseScreenBackup() { SAFERELEASE(m_ScreenBackup); }

void CKDX9RasterizerContext::ReleaseVertexDeclarations()
{
    for (auto it = m_VertexDeclarations.Begin(); it != m_VertexDeclarations.End(); ++it)
    {
        LPDIRECT3DVERTEXDECLARATION9 decl = *it;
        SAFERELEASE(decl);
    }
    m_VertexDeclarations.Clear();
}

CKDWORD CKDX9RasterizerContext::DX9PresentInterval(DWORD PresentInterval)
{
    // Map the input interval to D3D9 presentation intervals
    switch (PresentInterval)
    {
        case 0: return D3DPRESENT_INTERVAL_IMMEDIATE;
        case 1: return D3DPRESENT_INTERVAL_ONE;
        case 2: return D3DPRESENT_INTERVAL_TWO;
        case 3: return D3DPRESENT_INTERVAL_THREE;
        case 4: return D3DPRESENT_INTERVAL_FOUR;
        default: return D3DPRESENT_INTERVAL_DEFAULT;
    }
}

CKBOOL CKDX9RasterizerContext::LoadSurface(const D3DSURFACE_DESC &ddsd, const D3DLOCKED_RECT &LockRect, const VxImageDescEx &SurfDesc)
{
    VxImageDescEx desc;
    desc.Size = sizeof(VxImageDescEx);
    VxPixelFormat2ImageDesc(D3DFormatToVxPixelFormat(ddsd.Format), desc);
    desc.Width = ddsd.Width;
    desc.Height = ddsd.Height;
    desc.BytesPerLine = LockRect.Pitch;
    desc.Image = static_cast<XBYTE *>(LockRect.pBits);
    VxDoBlit(SurfDesc, desc);
    return TRUE;
}

#pragma warning(disable : 4035)

_inline unsigned long GetMSB(unsigned long data)
{
    _asm
    {
        mov eax,data
        bsr eax,eax
    }
}

#pragma warning(default : 4035)

LPDIRECT3DSURFACE9 CKDX9RasterizerContext::GetTempZBuffer(int Width, int Height)
{
    CKDWORD index = GetMSB(Height) << 4 | GetMSB(Width);
    if (index > 0xFF)
        return NULL;

    LPDIRECT3DSURFACE9 surface = m_TempZBuffers[index];
    if (surface)
        return surface;

    if (FAILED(m_Device->CreateDepthStencilSurface(Width, Height, m_PresentParams.AutoDepthStencilFormat,
                                                   D3DMULTISAMPLE_NONE, 0, TRUE, &surface, NULL)))
        return NULL;

    m_TempZBuffers[index] = surface;
    return surface;
}
