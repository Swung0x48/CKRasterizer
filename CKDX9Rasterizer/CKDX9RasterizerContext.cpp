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

    CKRECT rect;
    if (Window)
    {
        VxGetWindowRect(Window, &rect);
        WIN_HANDLE parent = VxGetParent(Window);
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&rect));
        VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&rect.right));
    }
    if (Fullscreen)
    {
        LONG prevStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
        SetWindowLongA((HWND)Window, GWL_STYLE, prevStyle & ~WS_CHILDWINDOW);
    }

    CKDX9RasterizerDriver *driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
    ZeroMemory(&m_PresentParams, sizeof(m_PresentParams));
    m_PresentParams.hDeviceWindow = (HWND)Window;
    m_PresentParams.BackBufferWidth = Width;
    m_PresentParams.BackBufferHeight = Height;
    m_PresentParams.BackBufferCount = 2; // Triple buffering
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

    if (m_PresentParams.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
    {
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
    }

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

    HRESULT hr;

#ifdef USE_D3D9EX
    hr = m_Owner->m_D3D9->CreateDeviceEx(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                         behaviorFlags, &m_PresentParams, Fullscreen ? &displayMode : NULL, &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDeviceEx(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                             behaviorFlags, &m_PresentParams, Fullscreen ? &displayMode : NULL, &m_Device);
    }
#else
    hr = m_Owner->m_D3D9->CreateDevice(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow, behaviorFlags, &m_PresentParams, &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDevice(driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow, behaviorFlags, &m_PresentParams, &m_Device);
    }
#endif

    if (Fullscreen)
    {
        LONG prevStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
        SetWindowLongA((HWND)Window, GCLP_HMODULE, prevStyle | WS_CHILDWINDOW);
    }
    else if (Window && !Fullscreen)
    {
        VxMoveWindow(Window, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
    }

    if (FAILED(hr))
    {
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    m_Window = (HWND)Window;
    m_PosX = PosX;
    m_PosY = PosY;
    m_Fullscreen = Fullscreen;

    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        VxImageDescEx desc;
        D3DSURFACE_DESC surfaceDesc;
        pBackBuffer->GetDesc(&surfaceDesc);
        pBackBuffer->Release();
        pBackBuffer = NULL;
        m_PixelFormat = D3DFormatToVxPixelFormat(surfaceDesc.Format);
        VxPixelFormat2ImageDesc(m_PixelFormat, desc);
        m_Bpp = desc.BitsPerPixel;
        m_Width = surfaceDesc.Width;
        m_Height = surfaceDesc.Height;
    }

    IDirect3DSurface9 *pStencilSurface = NULL;
    if (SUCCEEDED(m_Device->GetDepthStencilSurface(&pStencilSurface)))
    {
        D3DSURFACE_DESC desc;
        pStencilSurface->GetDesc(&desc);
        pStencilSurface->Release();
        pStencilSurface = NULL;
        m_ZBpp = DepthBitPerPixelFromFormat(desc.Format, &m_StencilBpp);
    }

    SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, TRUE);
    SetRenderState(VXRENDERSTATE_LOCALVIEWER, TRUE);
    SetRenderState(VXRENDERSTATE_COLORVERTEX, FALSE);

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
        else if (SUCCEEDED(hr))
        {
            m_Width = Width;
            m_Height = Height;
        }
        else
        {
            m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
            hr = m_Device->Reset(&m_PresentParams);
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
    if (hr == D3DERR_DEVICELOST)
    {
        hr = m_Device->TestCooperativeLevel();
        if (hr == D3DERR_DEVICENOTRESET)
        {
            Resize(m_PosX, m_PosY, m_Width, m_Height);
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
#ifdef ENABLE_TRACY
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
    D3DLIGHT9 lightData;
    switch (data->Type)
    {
        case VX_LIGHTDIREC:
            lightData.Type = D3DLIGHT_DIRECTIONAL;
            break;
        case VX_LIGHTPOINT:
        case VX_LIGHTPARA:
            lightData.Type = D3DLIGHT_POINT;
            break;
        case VX_LIGHTSPOT:
            lightData.Type = D3DLIGHT_SPOT;
        default:
            return FALSE;
    }

    lightData.Range = data->Range;
    lightData.Attenuation0 = data->Attenuation0;
    lightData.Attenuation1 = data->Attenuation1;
    lightData.Attenuation2 = data->Attenuation2;
    lightData.Ambient.a = data->Ambient.a;
    lightData.Ambient.r = data->Ambient.r;
    lightData.Ambient.g = data->Ambient.g;
    lightData.Ambient.b = data->Ambient.b;
    lightData.Diffuse.a = data->Diffuse.a;
    lightData.Diffuse.r = data->Diffuse.r;
    lightData.Diffuse.g = data->Diffuse.g;
    lightData.Diffuse.b = data->Diffuse.b;
    lightData.Position.x = data->Position.x;
    lightData.Position.y = data->Position.y;
    lightData.Position.z = data->Position.z;
    lightData.Direction.x = data->Direction.x;
    lightData.Direction.y = data->Direction.y;
    lightData.Direction.z = data->Direction.z;
    lightData.Falloff = data->Falloff;
    lightData.Specular.a = data->Specular.a;
    lightData.Specular.r = data->Specular.r;
    lightData.Specular.g = data->Specular.g;
    lightData.Specular.b = data->Specular.b;
    lightData.Theta = data->InnerSpotCone;
    lightData.Phi = data->OuterSpotCone;

    if (data && Light < 128)
        m_CurrentLightData[Light] = *data;

    ConvertAttenuationModelFromDX5(lightData.Attenuation0, lightData.Attenuation1, lightData.Attenuation2, data->Range);
    return SUCCEEDED(m_Device->SetLight(Light, &lightData));
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
    CKDWORD UnityMatrixMask = 0;
    D3DTRANSFORMSTATETYPE D3DTs = (D3DTRANSFORMSTATETYPE)Type;
    switch (Type)
    {
        case VXMATRIX_WORLD:
            m_WorldMatrix = Mat;
            UnityMatrixMask = WORLD_TRANSFORM;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            D3DTs = D3DTS_WORLD;
            break;
        case VXMATRIX_VIEW:
            m_ViewMatrix = Mat;
            UnityMatrixMask = VIEW_TRANSFORM;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate = 0;
            D3DTs = D3DTS_VIEW;
            break;
        case VXMATRIX_PROJECTION:
            m_ProjectionMatrix = Mat;
            UnityMatrixMask = PROJ_TRANSFORM;
            m_MatrixUptodate = 0;
            D3DTs = D3DTS_PROJECTION;
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
    }
    else
    {
        m_UnityMatrixMask &= ~UnityMatrixMask;
    }
    return SUCCEEDED(m_Device->SetTransform(D3DTs, (const D3DMATRIX *)&Mat));
}

CKBOOL CKDX9RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    if (m_StateCache[State].Flag != 0)
        return TRUE;

    if (m_StateCache[State].Valid != 0 && m_StateCache[State].Value == Value)
    {
        ++m_RenderStateCacheHit;
        return TRUE;
    }

    ++m_RenderStateCacheMiss;
    m_StateCache[State].Value = Value;
    m_StateCache[State].Valid = 1;

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
            m_StateCache[VXRENDERSTATE_CULLMODE].Valid = 0;
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
    CKDX9TextureDesc *desc = NULL;
    HRESULT hr = E_FAIL;
    if (Texture != 0 && Texture < m_Textures.Size() &&
        (desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture])) != NULL && desc->DxTexture != NULL)
    {
        hr = m_Device->SetTexture(Stage, desc->DxTexture);
        if (Stage == 0)
        {
            hr = m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            assert(SUCCEEDED(hr));
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            assert(SUCCEEDED(hr));
            m_Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
            assert(SUCCEEDED(hr));
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            assert(SUCCEEDED(hr));
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            assert(SUCCEEDED(hr));
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            assert(SUCCEEDED(hr));
        }
    }
    else
    {
        hr = m_Device->SetTexture(Stage, NULL);
        if (Stage == 0)
        {
            hr = m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            assert(SUCCEEDED(hr));
            hr = m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            assert(SUCCEEDED(hr));
            hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            assert(SUCCEEDED(hr));
            hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
            assert(SUCCEEDED(hr));
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

#ifdef ENABLE_TRACY
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
#ifdef ENABLE_TRACY
        ZoneScopedN("Lock");
#endif
        hr = vertexBufferDesc->DxBuffer->Lock(vertexSize * vertexBufferDesc->m_CurrentVCount,
                                              vertexSize * data->VertexCount, &ppbData, D3DLOCK_NOOVERWRITE);
        startIndex = vertexBufferDesc->m_CurrentVCount;
        vertexBufferDesc->m_CurrentVCount += data->VertexCount;
    }
    else
    {
#ifdef ENABLE_TRACY
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

#ifdef ENABLE_TRACY
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

#ifdef ENABLE_TRACY
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

    if ((desc->Flags & (CKRST_TEXTURE_CUBEMAP | CKRST_TEXTURE_RENDERTARGET)) != 0)
        return TRUE;

    HRESULT hr;

    int actualMipLevel = (miplevel < 0) ? 0 : miplevel;
    D3DSURFACE_DESC surfaceDesc;
    hr = desc->DxTexture->GetLevelDesc(actualMipLevel, &surfaceDesc);
    assert(SUCCEEDED(hr));

    IDirect3DSurface9 *pSurface = NULL;

    if ((surfaceDesc.Format == D3DFMT_DXT1 ||
        surfaceDesc.Format == D3DFMT_DXT2 ||
        surfaceDesc.Format == D3DFMT_DXT3 ||
        surfaceDesc.Format == D3DFMT_DXT4 ||
        surfaceDesc.Format == D3DFMT_DXT5) &&
        (D3DXLoadSurfaceFromSurface && D3DXLoadSurfaceFromMemory))
    {
        IDirect3DSurface9 *pSurfaceLevel = NULL;
        desc->DxTexture->GetSurfaceLevel(actualMipLevel, &pSurfaceLevel);
        if (!pSurfaceLevel)
            return FALSE;

        RECT srcRect{0, 0, SurfDesc.Height, SurfDesc.Width};
        D3DFORMAT format = VxPixelFormatToD3DFormat(VxImageDesc2PixelFormat(SurfDesc));
        hr = D3DXLoadSurfaceFromMemory(pSurfaceLevel, NULL, NULL, SurfDesc.Image, format, SurfDesc.BytesPerLine, NULL, &srcRect, D3DX_FILTER_LINEAR, 0);
        assert(SUCCEEDED(hr));

        CKDWORD mipMapCount = m_Textures[Texture]->MipMapCount;
        if (miplevel == -1 && mipMapCount > 0)
        {
            for (int i = 1; i < mipMapCount + 1; ++i)
            {
                desc->DxTexture->GetSurfaceLevel(i, &pSurface);
                hr = D3DXLoadSurfaceFromSurface(pSurface, NULL, NULL, pSurfaceLevel, NULL, NULL, D3DX_FILTER_BOX, 0);
                assert(SUCCEEDED(hr));
                pSurfaceLevel->Release();
            }
        }
        else
        {
            pSurface = pSurfaceLevel;
        }

        if (pSurface)
            pSurface->Release();

#if LOGGING && LOG_LOADTEXTURE
        if (FAILED(hr))
            fprintf(stderr, "LoadTexture (DXTC) failed with %x\n", hr);
#endif
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
    if (FAILED(desc->DxTexture->LockRect(actualMipLevel, &lockRect, NULL, 0)))
    {
#if LOGGING && LOG_LOADTEXTURE
        fprintf(stderr, "LoadTexture (Locking) failed with %x\n", hr);
#endif
        return FALSE;
    }

    LoadSurface(surfaceDesc, lockRect, src);

    hr = desc->DxTexture->UnlockRect(actualMipLevel);
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

            if (FAILED(desc->DxTexture->GetLevelDesc(i, &surfaceDesc)))
                break;

            if (FAILED(desc->DxTexture->LockRect(i, &lockRect, NULL, NULL)))
            {
#if LOGGING && LOG_LOADTEXTURE
                fprintf(stderr, "LoadTexture (Mipmap generation) failed with %x\n", hr);
#endif
                return FALSE;
            }

            LoadSurface(surfaceDesc, lockRect, dst);

            hr = desc->DxTexture->UnlockRect(i);
            assert(SUCCEEDED(hr));
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

    RECT destRect;
    if (Dest)
        SetRect(&destRect, Dest->left, Dest->top, Dest->right, Dest->bottom);
    else
        SetRect(&destRect, 0, 0, desc->Format.Width, desc->Format.Height);

    RECT srcRect;
    if (Src)
        SetRect(&srcRect, Src->left, Src->top, Src->right, Src->bottom);
    else
        SetRect(&srcRect, 0, 0, desc->Format.Width, desc->Format.Height);

    HRESULT hr;

    IDirect3DSurface9 *backBuffer = NULL;
    hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    assert(SUCCEEDED(hr));

    IDirect3DSurface9 *textureSurface = NULL;
    hr = desc->DxTexture->GetSurfaceLevel(0, &textureSurface);
    assert(SUCCEEDED(hr));

    POINT pt = {destRect.left, destRect.top};

    if (backBuffer && textureSurface)
    {
        hr = m_Device->UpdateSurface(textureSurface, &srcRect, backBuffer, &pt);
        assert(SUCCEEDED(hr));
        textureSurface->Release();
        textureSurface = NULL;
    }
    else if (textureSurface)
    {
        textureSurface->Release();
    }

    if (FAILED(hr) && (desc->Flags & CKRST_TEXTURE_MANAGED) != 0)
    {
        desc->DxTexture->Release();
        desc->DxTexture = NULL;

        if (SUCCEEDED(m_Device->CreateTexture(desc->Format.Width, desc->Format.Height, 1, D3DUSAGE_RENDERTARGET,
                                              m_PresentParams.BackBufferFormat, D3DPOOL_DEFAULT, &desc->DxTexture, NULL)))
        {
            D3DFormatToTextureDesc(m_PresentParams.BackBufferFormat, desc);
            desc->Flags &= ~CKRST_TEXTURE_MANAGED;
            desc->Flags |= (CKRST_TEXTURE_RENDERTARGET | CKRST_TEXTURE_VALID);
            desc->DxTexture->GetSurfaceLevel(0, &textureSurface);
            hr = m_Device->UpdateSurface(backBuffer, &srcRect, textureSurface, &pt);
            assert(SUCCEEDED(hr));

            if (textureSurface)
                textureSurface->Release();

            if (backBuffer)
                backBuffer->Release();
            return SUCCEEDED(hr);
        }
    }

    return FALSE;
}

CKBOOL CKDX9RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face, CKBOOL GenerateMipMap)
{
    EndScene();

    if (!TextureObject)
    {
        if (m_DefaultBackBuffer)
        {
            HRESULT hr = m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
            assert(SUCCEEDED(hr));
            hr = m_DefaultBackBuffer->Release();
            assert(SUCCEEDED(hr));
            m_DefaultBackBuffer = NULL;
            hr = m_DefaultDepthBuffer->Release();
            assert(SUCCEEDED(hr));
            m_DefaultBackBuffer = NULL;
            if (m_CurrentTextureIndex >= m_Textures.Size())
                return SUCCEEDED(hr);
            CKTextureDesc *desc = m_Textures[m_CurrentTextureIndex];
            if (desc)
            {
                desc->Flags &= ~CKRST_TEXTURE_RENDERTARGET;
                m_CurrentTextureIndex = 0;
            }
            return SUCCEEDED(hr);
        }
        return FALSE;
    }

    if (TextureObject >= m_Textures.Size())
        return FALSE;
    if (!m_Device)
        return FALSE;
    if (m_DefaultBackBuffer)
        return FALSE;

    CKBOOL cubemap = FALSE;
    if (Height < 0)
    {
        cubemap = TRUE;
        Height = Width;
    }

    if (!m_Textures[TextureObject])
    {
        CKDX9TextureDesc *desc = new CKDX9TextureDesc;
        desc->Format.Width = (Width != 0) ? Width : 256;
        desc->Format.Height = (Height != 0) ? Height : 256;
        m_Textures[TextureObject] = desc;
    }
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[TextureObject]);

    HRESULT hr = m_Device->GetRenderTarget(0, &m_DefaultBackBuffer);
    if (FAILED(hr) || !m_DefaultBackBuffer)
        return FALSE;

    hr = m_Device->GetDepthStencilSurface(&m_DefaultDepthBuffer);
    if (FAILED(hr) || !m_DefaultDepthBuffer)
    {
        m_DefaultDepthBuffer->Release();
        m_DefaultDepthBuffer = NULL;
        return FALSE;
    }

    for (int i = 0; i < m_Driver->m_3DCaps.MaxNumberTextureStage; ++i)
    {
        hr = m_Device->SetTexture(i, NULL);
        assert(SUCCEEDED(hr));
    }

    if ((cubemap || desc->DxRenderTexture) && desc->DxTexture)
    {
        IDirect3DSurface9 *surface = NULL;
        D3DRESOURCETYPE type = desc->DxTexture->GetType();
        if (cubemap)
        {
            if (type == D3DRTYPE_CUBETEXTURE)
            {
                hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, 0, &surface);
                assert(SUCCEEDED(hr));
            }
        }
        else
        {
            desc->DxRenderTexture = desc->DxTexture;
            hr = desc->DxTexture->GetSurfaceLevel(0, &surface);
            assert(SUCCEEDED(hr));
        }

        IDirect3DSurface9 *zbuffer = GetTempZBuffer(desc->Format.Width, desc->Format.Height);
        D3DSURFACE_DESC surfaceDesc = {};
        if (surface)
        {
            hr = surface->GetDesc(&surfaceDesc);
            assert(SUCCEEDED(hr));

            hr = (surfaceDesc.Usage & D3DUSAGE_RENDERTARGET) ? m_Device->SetRenderTarget(0, surface) : -1;
            surface->Release();
            if (SUCCEEDED(hr))
            {
                desc->Flags &= ~CKRST_TEXTURE_MANAGED;
                desc->Flags |= (CKRST_TEXTURE_RENDERTARGET | CKRST_TEXTURE_VALID);
                m_CurrentTextureIndex = TextureObject;
                desc->MipMapCount = 0;
                return TRUE;
            }
        }
    }

    desc->Flags &= ~CKRST_TEXTURE_VALID;
    if (desc->DxTexture)
        desc->DxTexture->Release();
    if (desc->DxRenderTexture)
        desc->DxRenderTexture->Release();
    desc->DxTexture = NULL;
    desc->DxRenderTexture = NULL;
    desc->MipMapCount = 0;

    if (cubemap)
    {
        hr = m_Device->CreateCubeTexture(desc->Format.Width, 1, D3DUSAGE_RENDERTARGET, m_PresentParams.BackBufferFormat,
                                         D3DPOOL_DEFAULT, &desc->DxCubeTexture, NULL);
        if (FAILED(hr))
        {
            desc->Flags &= ~CKRST_TEXTURE_VALID;
            hr = m_DefaultBackBuffer->Release();
            assert(SUCCEEDED(hr));
            m_DefaultBackBuffer = NULL;
            return FALSE;
        }
    }
    else
    {
        hr = m_Device->CreateTexture(desc->Format.Width, desc->Format.Height, 1, D3DUSAGE_RENDERTARGET,
                                     m_PresentParams.BackBufferFormat, D3DPOOL_DEFAULT, &desc->DxTexture, NULL);
        if (FAILED(hr))
        {
            desc->Flags &= ~CKRST_TEXTURE_VALID;
            hr = m_DefaultBackBuffer->Release();
            assert(SUCCEEDED(hr));
            m_DefaultBackBuffer = NULL;
            return FALSE;
        }

        hr = m_Device->CreateTexture(desc->Format.Width, desc->Format.Height, 1, D3DUSAGE_RENDERTARGET,
                                    m_PresentParams.BackBufferFormat, D3DPOOL_DEFAULT, &desc->DxRenderTexture, NULL);
        assert(SUCCEEDED(hr));
    }

    IDirect3DSurface9 *surface = NULL;
    if (cubemap)
    {
        hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, 0, &surface);
        assert(SUCCEEDED(hr));
    }
    else
    {
        hr = desc->DxRenderTexture->GetSurfaceLevel(0, &surface);
        assert(SUCCEEDED(hr));
    }

    IDirect3DSurface9 *zbuffer = GetTempZBuffer(desc->Format.Width, desc->Format.Height);
    hr = m_Device->SetRenderTarget(0, surface);
    if (surface)
        surface->Release();
    if (FAILED(hr))
    {
        desc->Flags &= ~CKRST_TEXTURE_VALID;
        m_DefaultBackBuffer->Release();
        m_DefaultBackBuffer = NULL;
        m_DefaultDepthBuffer->Release();
        m_DefaultDepthBuffer = NULL;
        return FALSE;
    }

    D3DFormatToTextureDesc(m_PresentParams.BackBufferFormat, desc);
    desc->Flags &= ~CKRST_TEXTURE_MANAGED;
    desc->Flags |= CKRST_TEXTURE_VALID | CKRST_TEXTURE_RENDERTARGET;
    if (cubemap)
        desc->Flags |= CKRST_TEXTURE_CUBEMAP;

    m_CurrentTextureIndex = TextureObject;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    if (Sprite >= m_Sprites.Size())
        return FALSE;

    CKSpriteDesc *sprite = m_Sprites[Sprite];
    if (!sprite)
        return FALSE;

    // if (sprite->Textures.Size() < 16)
    //     return FALSE;
    if (src->GetWidth() <= 0.0f)
        return FALSE;
    if (src->right < 0.0f)
        return FALSE;
    if (sprite->Format.Width < src->left)
        return FALSE;
    if (src->GetHeight() < 0.0f)
        return FALSE;
    if (src->bottom < 0.0f)
        return FALSE;
    if (sprite->Format.Height <= src->top)
        return FALSE;
    if (dst->GetWidth() <= 0.0f)
        return FALSE;
    if (dst->right < 0.0f)
        return FALSE;
    if (m_Width <= dst->left)
        return FALSE;
    if (dst->left <= 0.0f)
        return FALSE;
    if (dst->bottom < 0.0f)
        return FALSE;
    if (m_Height <= dst->top)
        return FALSE;

    HRESULT hr;
    hr = m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    assert(SUCCEEDED(hr));
    hr = m_Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    assert(SUCCEEDED(hr));

    CKBOOL ret;
    ret = SetRenderState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_ZENABLE, FALSE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_WRAP0, 0);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_CLIPPING, FALSE);
    assert(ret);
    ret = SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, FALSE);
    assert(ret);

    D3DVIEWPORT9 viewport;
    viewport.Height = m_Height;
    viewport.Width = m_Width;
    viewport.X = 0;
    viewport.Y = 0;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    hr = m_Device->SetViewport(&viewport);
    assert(SUCCEEDED(hr));

    CKDWORD startVertex = 0;
    int count = 4 * sprite->Textures.Size();
    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[GetDynamicVertexBuffer(CKRST_VF_TLVERTEX, count, 32, 1)]);
    if (!vb)
        return FALSE;

    void *pBuf = NULL;
    if (vb->m_CurrentVCount + count <= vb->m_MaxVertexCount)
    {
        hr = vb->DxBuffer->Lock(32 * vb->m_CurrentVCount, 32 * count, &pBuf, D3DLOCK_NOOVERWRITE);
        assert(SUCCEEDED(hr));
        startVertex = vb->m_CurrentVCount;
        vb->m_CurrentVCount = count + startVertex;
    }
    else
    {
        hr = vb->DxBuffer->Lock(0, 32 * count, &pBuf, D3DLOCK_DISCARD);
        assert(SUCCEEDED(hr));
        vb->m_CurrentVCount = count;
    }

    float widthRatio = dst->GetWidth() / src->GetWidth();
    float heightRatio = dst->GetHeight() / src->GetHeight();
    CKVertex *vbData = static_cast<CKVertex *>(pBuf);
    for (auto texture = sprite->Textures.Begin(); texture != sprite->Textures.End(); texture++, vbData += 4)
    {
        float tx = texture->x;
        float ty = texture->y;
        float tr = tx + texture->w;
        float tb = ty + texture->h;
        if (tx <= src->right && ty <= src->bottom && tr >= src->left && tb >= src->top)
        {
            float tu2 = 1.0f;
            if (texture->w != texture->sw)
                tu2 = (float)texture->w / (float)texture->sw;

            float tv2 = 1.0f;
            if (texture->h != texture->sh)
                tv2 = (float)texture->h / (float)texture->sh;
            
            if (src->right < tr)
            {
                tr = src->right;
                tu2 = (src->right - tx) / texture->sw;
            }

            if (src->bottom < tb)
            {
                tb = src->bottom;
                tv2 = (src->bottom - ty) / texture->sh;
            }

            float tu = 0.0f;
            if (src->left <= tx)
            {
                tu = (src->left - tx) / texture->sw;
                tx = src->left;
            }

            float tv = 0.0f;
            if (src->top <= ty)
            {
                tv = (src->top - ty) / texture->sh;
                ty = src->top;
            }

            float tu1 = 0.25f / (float)(texture->sw * widthRatio) + tu;
            float tv1 = 0.25f / (float)(texture->sh * heightRatio) + tv;

            vbData[0].Diffuse = (R_MASK | G_MASK | B_MASK | A_MASK);
            vbData[1].Diffuse = (R_MASK | G_MASK | B_MASK | A_MASK);
            vbData[2].Diffuse = (R_MASK | G_MASK | B_MASK | A_MASK);
            vbData[3].Diffuse = (R_MASK | G_MASK | B_MASK | A_MASK);
            vbData[0].Specular = A_MASK;
            vbData[1].Specular = A_MASK;
            vbData[2].Specular = A_MASK;
            vbData[3].Specular = A_MASK;
            vbData[0].tu = tu1;
            vbData[0].tv = tv1;
            vbData[1].tu = tu1;
            vbData[1].tv = tv2;
            vbData[2].tu = tu2;
            vbData[2].tv = tv2;
            vbData[3].tu = tu2;
            vbData[3].tv = tv1;
            vbData[0].V = VxVector4((tx - src->left) * widthRatio + dst->left, (ty - src->top) * heightRatio + dst->top, 0.0f, 1.0f);
            vbData[1].V = VxVector4(vbData[0].V.x, (tb - src->top) * heightRatio + dst->top, 0.0f, 1.0f);
            vbData[2].V = VxVector4((tr - src->left) * widthRatio + dst->left, vbData[1].V.y, 0.0f, 1.0f);
            vbData[3].V = VxVector4(vbData[2].V.x, vbData[0].V.y, 0.0f, 1.0f);
        }
    }

    hr = vb->DxBuffer->Unlock();
    assert(SUCCEEDED(hr));

    m_CurrentVertexBufferCache = NULL;
    SetupStreams(vb->DxBuffer, CKRST_VF_TLVERTEX, 32);

    for (auto texture = sprite->Textures.Begin(); texture != sprite->Textures.End(); ++texture)
    {
        if (texture->x <= src->right && texture->y <= src->bottom && texture->x + texture->w >= src->left && texture->y + texture->h >= src->top)
        {
            hr = m_Device->SetTexture(0, static_cast<CKDX9TextureDesc *>(m_Textures[texture->IndexTexture])->DxTexture);
            assert(SUCCEEDED(hr));
            hr = m_Device->DrawPrimitive(D3DPT_TRIANGLEFAN, startVertex, 2);
            assert(SUCCEEDED(hr));
        }
    }

    hr = m_Device->SetStreamSource(0, NULL, NULL, NULL);
    assert(SUCCEEDED(hr));

    hr = m_Device->SetViewport((const D3DVIEWPORT9 *)&m_ViewportData);
    assert(SUCCEEDED(hr));

    SetRenderState(VXRENDERSTATE_ZENABLE, TRUE);
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
    int bbp = 0;
    HRESULT hr;

    switch (buffer)
    {
        case VXBUFFER_BACKBUFFER:
            {
                hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
                assert(SUCCEEDED(hr));
                if (!surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                assert(SUCCEEDED(hr));

                VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(desc.Format);
                VxPixelFormat2ImageDesc(vxpf, img_desc);
                break;
            }
        case VXBUFFER_ZBUFFER:
            {
                hr = m_Device->GetDepthStencilSurface(&surface);
                assert(SUCCEEDED(hr));
                if (!surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                assert(SUCCEEDED(hr));

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
                        bbp = 2;
                        break;
                    case D3DFMT_D32:
                    case D3DFMT_D24S8:
                    case D3DFMT_D24X8:
                    case D3DFMT_D24X4S4:
                        bbp = 4;
                        break;
                    default:
                        break;
                }
            }
        case VXBUFFER_STENCILBUFFER:
            {
                hr = m_Device->GetDepthStencilSurface(&surface);
                assert(SUCCEEDED(hr));
                if (!surface)
                    return 0;

                hr = surface->GetDesc(&desc);
                assert(SUCCEEDED(hr));

                D3DFORMAT d3dFormat = desc.Format;
                img_desc.BitsPerPixel = 32;
                img_desc.AlphaMask = 0;
                img_desc.BlueMask = B_MASK;
                img_desc.GreenMask = G_MASK;
                img_desc.RedMask = R_MASK;
                if (d3dFormat != D3DFMT_D15S1)
                {
                    if (d3dFormat != D3DFMT_D24S8 && d3dFormat != D3DFMT_D24X4S4)
                    {
                        surface->Release();
                        return 0;
                    }
                    bbp = 4;
                }
                else
                {
                    bbp = 2;
                }
                break;
            }
        default:
            return 0;
    }

    UINT right, left, top, bottom;
    if (rect)
    {
        right = (rect->right > desc.Width) ? desc.Width : rect->right;
        left = (rect->left < 0) ? 0 : rect->left;
        top = (rect->top < 0) ? 0 : rect->top;
        bottom = (rect->bottom > desc.Height) ? desc.Height : rect->bottom;
    }
    else
    {
        top = 0;
        bottom = desc.Height;
        left = 0;
        right = desc.Width;
    }

    UINT width = right - left;
    UINT height = bottom - top;
    img_desc.Width = width;
    img_desc.Height = height;
    int bytesPerPixel = img_desc.BitsPerPixel / 8;
    img_desc.BytesPerLine = width * bytesPerPixel;
    if (img_desc.BytesPerLine != 0)
    {
        IDirect3DSurface9 *imageSurface = NULL;
        D3DLOCKED_RECT lockedRect;
        if (FAILED(m_Device->CreateOffscreenPlainSurface(width, height, desc.Format, D3DPOOL_SCRATCH, &imageSurface, NULL)))
        {
            imageSurface = surface;
            surface->AddRef();
        }
        else if (FAILED(m_Device->UpdateSurface(surface, NULL, imageSurface, NULL)))
        {
            imageSurface->Release();
            surface->Release();
            return 0;
        }

        if (FAILED(imageSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY)))
        {
            imageSurface->Release();
            surface->Release();
            return 0;
        }

        BYTE *pBits = &((BYTE *)lockedRect.pBits)[top * lockedRect.Pitch + left * bytesPerPixel];
        BYTE *image = img_desc.Image;

        if (bbp == 2)
        {
            if (top < bottom)
            {
                for (int i = 0; i < height; ++i)
                {
                    if (left < bottom)
                    {
                        for (int j = 0; j < width; ++j)
                        {
                            *image = *pBits;
                            image += 4;
                            pBits += 2;
                        }
                    }
                    image += img_desc.BytesPerLine;
                    pBits += lockedRect.Pitch;
                }
            }
        }
        else
        {
            if (top < bottom)
            {
                for (int i = 0; i < height; ++i)
                {
                    memcpy(image, pBits, img_desc.BytesPerLine);
                    image += img_desc.BytesPerLine;
                    pBits += lockedRect.Pitch;
                }
            }
        }

        hr = imageSurface->UnlockRect();
        assert(SUCCEEDED(hr));

        hr = imageSurface->Release();
        assert(SUCCEEDED(hr));
    }

    surface->Release();
    assert(SUCCEEDED(hr));
    return img_desc.BytesPerLine * img_desc.Height;
}

int CKDX9RasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    HRESULT hr;
    if (!img_desc.Image)
        return 0;
    if (buffer != VXBUFFER_BACKBUFFER)
        return 0;

    IDirect3DSurface9 *backBuffer;
    hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    assert(SUCCEEDED(hr));
    if (FAILED(hr) || !backBuffer)
        return 0;

    D3DSURFACE_DESC desc = {};
    hr = backBuffer->GetDesc(&desc);
    assert(SUCCEEDED(hr));

    VxImageDescEx imgDesc;
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(desc.Format);
    VxPixelFormat2ImageDesc(vxpf, imgDesc);

    int left, top, right, bottom, width, height;
    if (rect)
    {
        left = rect->left;
        top = rect->top;
        right = rect->right;
        bottom = rect->bottom;
        width = rect->left;
        height = bottom;
        if (top < 0)
        {
            top = 0;
        }
        if (bottom > desc.Height)
        {
            bottom = desc.Height;
            height = desc.Height;
        }
        if (left < 0)
        {
            left = 0;
            width = 0;
        }
        if (right > desc.Width)
        {
            right = desc.Width;
        }
    }
    else
    {
        bottom = desc.Height;
        right = desc.Width;
        top = 0;
        left = 0;
        height = desc.Height;
        width = 0;
    }

    int bytesPerPixel = img_desc.BitsPerPixel / 8;

    IDirect3DSurface9 *surface = NULL;
    if ((img_desc.Width != right - left) ||
        (img_desc.Height != bottom - top) ||
        (img_desc.BitsPerPixel != imgDesc.BitsPerPixel) ||
        FAILED(m_Device->CreateOffscreenPlainSurface(img_desc.Width, img_desc.Height, desc.Format, D3DPOOL_SCRATCH, &surface, NULL)))
    {
            backBuffer->Release();
            return 0;
    }

    D3DLOCKED_RECT lockedRect;
    if (FAILED(surface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY)))
    {
        surface->Release();
        backBuffer->Release();
        return 0;
    }

    if (top < height)
    {
        BYTE *pBits = &((BYTE *)lockedRect.pBits)[top * lockedRect.Pitch + width * bytesPerPixel];
        BYTE *image = img_desc.Image;
        const int lines = bottom - top;
        for (int i = 0; i < lines; ++i)
        {
            memcpy(pBits, image, 4 * (img_desc.BytesPerLine / 4));
            BYTE *ptr = (BYTE *)&image[4 * (img_desc.BytesPerLine / 4)];
            image += img_desc.BytesPerLine;
            memcpy(&pBits[(img_desc.BytesPerLine >> 2) * 4], ptr, img_desc.BytesPerLine & 0x3);
            pBits += lockedRect.Pitch;
        }
    }

    hr = surface->UnlockRect();
    assert(SUCCEEDED(hr));

    hr = m_Device->UpdateSurface(backBuffer, NULL, surface, NULL);
    assert(SUCCEEDED(hr));

    hr = surface->Release();
    assert(SUCCEEDED(hr));

    hr = backBuffer->Release();
    assert(SUCCEEDED(hr));

    return img_desc.BytesPerLine * img_desc.Height;
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
                pCubeMapSurface->Release();
            }
        }
        else
        {
            pSurface = pCubeMapSurface;
        }

        if (pSurface)
            pSurface->Release();
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
                return FALSE;

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
    if (!param->NoMipMap)
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

    if (pZStencilSurface)
    {
        pZStencilSurface->Release();
        assert(SUCCEEDED(hr));
    }

    if (pBackBuffer)
    {
        hr = pBackBuffer->Release();
        assert(SUCCEEDED(hr));
    }
}

CKBOOL CKDX9RasterizerContext::InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc *VB,
                                                       CKDWORD StartIndex, CKDWORD VertexCount, WORD *indices,
                                                       int indexcount, CKBOOL Clip)
{
#ifdef ENABLE_TRACY
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
#ifdef ENABLE_TRACY
            ZoneScopedN("Lock");
#endif
            hr = desc->DxBuffer->Lock(2 * desc->m_CurrentICount, 2 * indexcount, &pbData, D3DLOCK_NOOVERWRITE);
            assert(SUCCEEDED(hr));
            ibstart = desc->m_CurrentICount;
            desc->m_CurrentICount += indexcount;
        }
        else
        {
#ifdef ENABLE_TRACY
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

    if (indexcount == 0)
        indexcount = VertexCount;
    switch (pType)
    {
        case VX_LINELIST:
            indexcount /= 2;
            break;
        case VX_LINESTRIP:
            indexcount -= 1;
            break;
        case VX_TRIANGLELIST:
            indexcount /= 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            indexcount -= 2;
            break;
        default:
            break;
    }

    if (!indices || pType == VX_POINTLIST)
    {
        hr = m_Device->DrawPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, indexcount);
        return SUCCEEDED(hr);
    }

    hr = m_Device->SetIndices(m_IndexBuffer[Clip]->DxBuffer);
    assert(SUCCEEDED(hr));

    return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, 0, VertexCount, ibstart, indexcount));
}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize)
{
#ifdef ENABLE_TRACY
    ZoneScopedN(__FUNCTION__);
#endif
    HRESULT hr;

    CKBOOL fixed = FALSE;

    if (m_CurrentVertexShaderCache != 0 && m_CurrentVertexShaderCache < m_VertexShaders.Size())
    {
        CKDX9VertexShaderDesc *desc = (CKDX9VertexShaderDesc *)m_VertexShaders[m_CurrentVertexShaderCache];
        if (desc->DxShader)
        {
            IDirect3DVertexDeclaration9 *pDecl = nullptr;

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
    if (Texture >= m_Textures.Size())
        return FALSE;

    if (m_Textures[Texture])
        return TRUE;

#if LOGGING && LOG_CREATETEXTURE
    fprintf(stderr, "create texture %d %dx%d %x\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height, DesiredFormat->Flags);
#endif

    CKDX9TextureDesc *desc = new CKDX9TextureDesc;
    desc->Flags = DesiredFormat->Flags;
    desc->Format = DesiredFormat->Format;
    desc->MipMapCount = DesiredFormat->MipMapCount;
    m_Textures[Texture] = desc;

    D3DFORMAT format = static_cast<CKDX9RasterizerDriver *>(m_Driver)->FindNearestTextureFormat(desc, m_PresentParams.BackBufferFormat, D3DUSAGE_DYNAMIC);
    HRESULT hr = m_Device->CreateTexture(desc->Format.Width, desc->Format.Height, desc->MipMapCount ? desc->MipMapCount : 1,
                                         D3DUSAGE_DYNAMIC, format, D3DPOOL_DEFAULT, &(desc->DxTexture), NULL);
#if LOGGING && LOG_CREATETEXTURE
    if (FAILED(hr))
    {
        fprintf(stderr, "create texture failure: src fmt bpp %d a@0x%x r@0x%x g@0x%x b@0x%x dst fmt 0x%x\n",
                desc->Format.BitsPerPixel, desc->Format.AlphaMask, desc->Format.RedMask, desc->Format.GreenMask, desc->Format.BlueMask, fmt);
    }
#endif
    return SUCCEEDED(hr);
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

    IDirect3DVertexDeclaration9 *pDecl = nullptr;
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

            m_DefaultBackBuffer->Release();
            m_DefaultBackBuffer = NULL;

            m_DefaultDepthBuffer->Release();
            m_DefaultDepthBuffer = NULL;
        }
    }

    for (int i = 0; i < m_Textures.Size(); ++i)
    {
        if (m_Textures[i] && (m_Textures[i]->Flags & CKRST_TEXTURE_MANAGED) == 0)
        {
            delete m_Textures[i];
            m_Textures[i] = NULL;
        }
    }

    ReleaseTempZBuffers();
    FlushObjects(60);
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
                if (m_TextureMinFilterStateBlocks[j][i])
                    m_TextureMinFilterStateBlocks[j][i]->Release();
                if (m_TextureMagFilterStateBlocks[j][i])
                    m_TextureMagFilterStateBlocks[j][i]->Release();
            }

            for (int k = 0; k < 10; k++)
            {
                if (m_TextureMapBlendStateBlocks[k][i])
                    m_TextureMapBlendStateBlocks[k][i]->Release();
            }
        }

        ZeroMemory(m_TextureMinFilterStateBlocks, sizeof(m_TextureMinFilterStateBlocks));
        ZeroMemory(m_TextureMagFilterStateBlocks, sizeof(m_TextureMagFilterStateBlocks));
        ZeroMemory(m_TextureMapBlendStateBlocks, sizeof(m_TextureMapBlendStateBlocks));
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

void CKDX9RasterizerContext::ReleaseScreenBackup()
{
    if (m_ScreenBackup)
        m_ScreenBackup->Release();
    m_ScreenBackup = NULL;
}

void CKDX9RasterizerContext::ReleaseVertexDeclarations()
{
    for (auto it = m_VertexDeclarations.Begin(); it != m_VertexDeclarations.End(); ++it)
    {
        LPDIRECT3DVERTEXDECLARATION9 decl = *it;
        SAFERELEASE(decl);
    }
    m_VertexDeclarations.Clear();
}

CKDWORD CKDX9RasterizerContext::DX9PresentInterval(DWORD PresentInterval) { return 0; }

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
