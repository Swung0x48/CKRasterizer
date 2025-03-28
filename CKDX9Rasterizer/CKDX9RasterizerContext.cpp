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

static inline CKDWORD GetMsb(CKDWORD num, CKDWORD max)
{
#define OPERAND_SIZE (sizeof(CKDWORD) * 8)
    CKDWORD i = OPERAND_SIZE - 1;
#ifdef WIN32
    __asm
    {
        mov eax, num
        bsr eax, eax
        mov i, eax
    }
#else
    if (num != 0)
        while (!(num & (1 << (OPERAND_SIZE - 1))))
        {
            num <<= 1;
            --i;
        }
#endif
    return (i > max) ? max : i;
#undef OPERAND_SIZE
}

CKBOOL CKDX9VertexShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKVertexShaderDesc *Format)
{
    if (!Ctx || !Ctx->m_Device)
        return FALSE;

    if (!m_Function || m_FunctionSize == 0)
        return FALSE;

    // Update ownership
    Owner = Ctx;

    // Re-create the shader if device is available
    SAFERELEASE(DxShader);
    HRESULT hr = Ctx->m_Device->CreateVertexShader(m_Function, &DxShader);

    return SUCCEEDED(hr);
}

CKDX9VertexShaderDesc::~CKDX9VertexShaderDesc()
{
    SAFERELEASE(DxShader);

    if (m_Function)
    {
        delete[] m_Function;
        m_Function = NULL;
    }
}

CKBOOL CKDX9PixelShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKPixelShaderDesc *Format)
{
    // Validate parameters
    if (!Ctx || !Ctx->m_Device)
        return FALSE;

    if (!m_Function || m_FunctionSize == 0)
        return FALSE;

    // Update ownership
    Owner = Ctx;

    // Re-create the shader if device is available
    SAFERELEASE(DxShader);
    HRESULT hr = Ctx->m_Device->CreatePixelShader(m_Function, &DxShader);

    return SUCCEEDED(hr);
}

CKDX9PixelShaderDesc::~CKDX9PixelShaderDesc()
{
    SAFERELEASE(DxShader);

    if (m_Function)
    {
        delete[] m_Function;
        m_Function = NULL;
    }
}

CKDX9RasterizerContext::CKDX9RasterizerContext(CKDX9RasterizerDriver *driver) :
    CKRasterizerContext(),
    m_Device(NULL),
    m_PresentParams(),
    m_DirectXData(),
    m_SoftwareVertexProcessing(FALSE),
    m_CurrentTextureIndex(0),
    m_DefaultBackBuffer(NULL),
    m_DefaultDepthBuffer(NULL),
    m_InCreateDestroy(TRUE),
    m_ScreenBackup(NULL),
    m_CurrentVertexShaderCache(0),
    m_CurrentVertexFormatCache(0),
    m_CurrentVertexBufferCache(NULL),
    m_CurrentVertexSizeCache(0),
    m_CurrentPixelShaderCache(0),
    m_StateCacheHitMask(),
    m_StateCacheMissMask(),
    m_Owner(NULL)
{
    if (!driver)
    {
        return; // Early return in case driver is null
    }

    // Set driver and owner
    m_Driver = driver;
    m_Owner = static_cast<CKDX9Rasterizer *>(driver->m_Owner);

    // Initialize arrays
    memset(m_IndexBuffer, 0, sizeof(m_IndexBuffer));
    memset(m_TranslatedRenderStates, 0, sizeof(m_TranslatedRenderStates));
    memset(m_TempZBuffers, 0, sizeof(m_TempZBuffers));

    // Initialize state block arrays
    memset(m_TextureMinFilterStateBlocks, 0, sizeof(m_TextureMinFilterStateBlocks));
    memset(m_TextureMagFilterStateBlocks, 0, sizeof(m_TextureMagFilterStateBlocks));
    memset(m_TextureMapBlendStateBlocks, 0, sizeof(m_TextureMapBlendStateBlocks));
}

CKDX9RasterizerContext::~CKDX9RasterizerContext()
{
    // Set flag to prevent recursive calls during destruction
    m_InCreateDestroy = TRUE;

    // If we're the fullscreen context, clear the reference
    if (m_Owner && m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;

    // Release anything bound to the device
    if (m_Device)
    {
        // End scene if it was active
        if (m_SceneBegined)
            EndScene();

        // Clear all texture stages
        for (int i = 0; i < 8; i++)
        {
            m_Device->SetTexture(i, NULL);
        }

        // Clear index buffer binding
        m_Device->SetIndices(NULL);

        // Clear vertex stream bindings
        for (UINT i = 0; i < 8; i++)
        {
            m_Device->SetStreamSource(i, NULL, 0, 0);
        }

        // Clear shader bindings
        SetVertexShader(NULL);
        SetPixelShader(NULL);

        // Clear render target binding
        if (m_DefaultBackBuffer)
        {
            m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
            if (m_DefaultDepthBuffer)
                m_Device->SetDepthStencilSurface(m_DefaultDepthBuffer);
        }
    }

    // Free state-specific resources
    ReleaseStateBlocks();
    ReleaseIndexBuffers();

    // Release temporary and cached resources
    ReleaseTempZBuffers();
    ReleaseScreenBackup();
    ReleaseVertexDeclarations();

    // Release major objects
    FlushObjects(CKRST_OBJ_ALL);

    // Release device-specific resources
    SAFERELEASE(m_DefaultBackBuffer);
    SAFERELEASE(m_DefaultDepthBuffer);

    // Finally release the device itself
    SAFERELEASE(m_Device);

    m_InCreateDestroy = FALSE;
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
#if (STEP) || (LOGGING)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
    m_InCreateDestroy = TRUE;

    // Store original window style for restoring later if needed
    LONG originalStyle = 0;
    CKRECT windowRect = {0, 0, 0, 0};

    // Get window position and handle parent relationship
    if (Window)
    {
        // Store original window style
        originalStyle = GetWindowLongA((HWND)Window, GWL_STYLE);

        // Get window position relative to parent
        VxGetWindowRect(Window, &windowRect);
        WIN_HANDLE parent = VxGetParent(Window);
        if (parent)
        {
            VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&windowRect));
            VxScreenToClient(parent, reinterpret_cast<CKPOINT *>(&windowRect.right));
        }

        // For fullscreen mode, remove child window style
        if (Fullscreen)
        {
            SetWindowLongA((HWND)Window, GWL_STYLE, originalStyle & ~WS_CHILDWINDOW);
        }
    }

    CKDX9RasterizerDriver *driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
    if (!driver || !driver->m_Inited)
    {
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

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
    
    // Use immediate presentation by default
    m_PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    // m_PresentParams.PresentationInterval = Fullscreen ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_DEFAULT;
    
    // Find appropriate formats for rendering
    m_PresentParams.BackBufferFormat = driver->FindNearestRenderTargetFormat(Bpp, !Fullscreen);
    if (m_PresentParams.BackBufferFormat == D3DFMT_UNKNOWN)
    {
        RestoreWindowStyle((HWND)Window, originalStyle, Fullscreen);
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    m_PresentParams.AutoDepthStencilFormat = driver->FindNearestDepthFormat(m_PresentParams.BackBufferFormat, Zbpp, StencilBpp);
    if (m_PresentParams.AutoDepthStencilFormat == D3DFMT_UNKNOWN)
    {
        RestoreWindowStyle((HWND)Window, originalStyle, Fullscreen);
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Configure multisampling
    ConfigureMultisampling();

    D3DDISPLAYMODEEX displayMode = {
        sizeof(D3DDISPLAYMODEEX),
        (UINT)Width,
        (UINT)Height,
        (UINT)RefreshRate,
        m_PresentParams.BackBufferFormat,
        D3DSCANLINEORDERING_PROGRESSIVE
    };

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

    HRESULT hr = S_OK;

    // Create the Direct3D device
#ifdef USE_D3D9EX
    hr = m_Owner->m_D3D9->CreateDeviceEx(
        driver->m_AdapterIndex, 
        D3DDEVTYPE_HAL, 
        (HWND)m_Owner->m_MainWindow,
        behaviorFlags, 
        &m_PresentParams, 
        Fullscreen ? &displayMode : NULL, 
        &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDeviceEx(
            driver->m_AdapterIndex, 
            D3DDEVTYPE_HAL, 
            (HWND)m_Owner->m_MainWindow,
            behaviorFlags, 
            &m_PresentParams, 
            Fullscreen ? &displayMode : NULL,
            &m_Device);
    }
#else
    hr = m_Owner->m_D3D9->CreateDevice(
        driver->m_AdapterIndex, 
        D3DDEVTYPE_HAL, 
        (HWND)m_Owner->m_MainWindow,
        behaviorFlags, 
        &m_PresentParams, 
        &m_Device);
    if (FAILED(hr) && m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = m_Owner->m_D3D9->CreateDevice(
            driver->m_AdapterIndex, 
            D3DDEVTYPE_HAL, 
            (HWND)m_Owner->m_MainWindow,
            behaviorFlags, 
            &m_PresentParams, 
            &m_Device);
    }
#endif

    // Restore window style if device creation failed
    if (FAILED(hr))
    {
        RestoreWindowStyle((HWND)Window, originalStyle, Fullscreen);
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Position the window appropriately
    if (Window)
    {
        if (Fullscreen)
        {
            // For fullscreen, restore window as a child if it was a child window
            if (originalStyle & WS_CHILDWINDOW)
            {
                SetWindowLongA((HWND)Window, GWL_STYLE, originalStyle);
            }
        }
        else
        {
            // For windowed mode, position the window correctly
            VxMoveWindow(Window, windowRect.left, windowRect.top,
                         windowRect.right - windowRect.left,
                         windowRect.bottom - windowRect.top, FALSE);
        }
    }

    // Store context configuration
    m_Window = (HWND)Window;
    m_PosX = PosX;
    m_PosY = PosY;
    m_Fullscreen = Fullscreen;

    // Update internal dimensions and formats from created device
    if (!UpdateDeviceProperties())
    {
        DestroyDevice();
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Set initial rendering states
    if (!InitializeDeviceStates())
    {
        DestroyDevice();
        m_InCreateDestroy = FALSE;
        return FALSE;
    }

    // Update DirectX data for external access
    UpdateDirectXData();
    
    // Initialize caches and object arrays
    FlushCaches();
    UpdateObjectArrays(m_Owner);
    ClearStreamCache();

    // Register as fullscreen context if in fullscreen mode
    if (m_Fullscreen)
        m_Owner->m_FullscreenContext = this;

    m_InCreateDestroy = FALSE;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (m_InCreateDestroy || !m_Device)
        return FALSE;

    // End any active scene
    if (m_SceneBegined)
        EndScene();

    ReleaseScreenBackup();

    // Update position if allowed
    if ((Flags & VX_RESIZE_NOMOVE) == 0)
    {
        m_PosX = PosX;
        m_PosY = PosY;
    }

    // Skip size changes if requested
    if ((Flags & VX_RESIZE_NOSIZE) != 0)
        return TRUE;

    // Calculate dimensions if not specified
    RECT rect;
    if (Width == 0 || Height == 0)
    {
        if (!m_Window || !GetClientRect((HWND)m_Window, &rect))
            return FALSE;

        Width = rect.right; // Don't subtract m_PosX - client rect is already relative
        Height = rect.bottom;
    }

    // Validate minimum dimensions
    if (Width < 1 || Height < 1)
        return FALSE;

    // Store old dimensions for fallback
    UINT oldWidth = m_Width;
    UINT oldHeight = m_Height;

    // Update presentation parameters with new size
    m_PresentParams.BackBufferWidth = Width;
    m_PresentParams.BackBufferHeight = Height;

    // Clean up resources that will be invalidated by Reset
    ReleaseVertexDeclarations();
    ReleaseStateBlocks();
    FlushNonManagedObjects();
    ClearStreamCache();

    // Configure multisampling
    ConfigureMultisampling();

    // Try to reset the device
    HRESULT hr = ResetDevice();
    if (SUCCEEDED(hr))
    {
        m_Width = Width;
        m_Height = Height;
    }
    else
    {
        // Reset failed - try to recover with original dimensions
        m_PresentParams.BackBufferWidth = oldWidth;
        m_PresentParams.BackBufferHeight = oldHeight;

        // Try without multisampling as a fallback
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = ResetDevice();

        if (FAILED(hr))
        {
            // Device is truly lost and can't be recovered yet
            // We'll need to wait for TestCooperativeLevel to return D3DERR_DEVICENOTRESET
            return FALSE;
        }
    }

    // Update DirectX data and state caches
    UpdateDirectXData();
    FlushCaches();

    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT *rects)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (m_InCreateDestroy || !m_Device)
        return FALSE;

    DWORD flags = 0;

    // In transparent mode, we skip clearing the color buffer
    if (!m_TransparentMode && (Flags & CKRST_CTXCLEAR_COLOR) && m_Bpp != 0)
        flags |= D3DCLEAR_TARGET;

    // Always handle depth and stencil clearing normally
    if ((Flags & CKRST_CTXCLEAR_STENCIL) && m_StencilBpp != 0)
        flags |= D3DCLEAR_STENCIL;

    if ((Flags & CKRST_CTXCLEAR_DEPTH) && m_ZBpp != 0)
        flags |= D3DCLEAR_ZBUFFER;

    if (flags == 0)
        return TRUE;

    // Validate rect count and pointers
    if (RectCount < 0)
        RectCount = 0;

    if (RectCount > 0 && !rects)
        RectCount = 0;

    // Perform the clear operation
    HRESULT hr = m_Device->Clear(RectCount, (D3DRECT *)rects, flags, Ccol, Z, Stencil);

    // If this was a full clear, reset the dirty rects
    if (RectCount == 0 && (Flags & CKRST_CTXCLEAR_COLOR))
    {
        ResetDirtyRects();
    }

    return SUCCEEDED(hr);
}

#if LOGGING && LOG_LOADTEXTURE
static int texture_used[100] = {0};
#endif
CKBOOL CKDX9RasterizerContext::BackToFront(CKBOOL vsync)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (m_InCreateDestroy || !m_Device)
        return FALSE;

    if (m_SceneBegined)
        EndScene();

    HRESULT hr = S_OK;

    if (vsync && !m_Fullscreen && m_CurrentTextureIndex == 0)
    {
        #ifdef TRACY_ENABLE
            ZoneScopedN("VSync");
        #endif
        D3DRASTER_STATUS status;
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
            // Check if device can be reset
            hr = m_Device->TestCooperativeLevel();
            if (hr == D3DERR_DEVICENOTRESET)
            {
                // Device can be reset now
                hr = ResetDevice();
                if (SUCCEEDED(hr))
                    return TRUE; // Successfully reset
            }

            // Device is lost and can't be reset yet
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

    if (!m_Device)
        return FALSE;

    // Begin the scene
    HRESULT hr = m_Device->BeginScene();
    if (SUCCEEDED(hr))
    {
        m_SceneBegined = TRUE;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

CKBOOL CKDX9RasterizerContext::EndScene()
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_SceneBegined)
        return TRUE;

    if (!m_Device)
    {
        // Force consistency even without a valid device
        m_SceneBegined = FALSE;
        return FALSE;
    }

    // End the scene
    HRESULT hr = m_Device->EndScene();
    // Update scene state regardless of success/failure
    // This ensures state consistency even if EndScene fails
    m_SceneBegined = FALSE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_Device || Light >= RST_MAX_LIGHT)
        return FALSE;

    return SUCCEEDED(m_Device->LightEnable(Light, Enable));
}

CKBOOL CKDX9RasterizerContext::SetMaterial(CKMaterialData *mat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_Device)
        return FALSE;

    if (!mat)
        return FALSE;

    m_CurrentMaterialData = *mat;

    D3DMATERIAL9 d3dMat;
    d3dMat.Diffuse.r = mat->Diffuse.r;
    d3dMat.Diffuse.g = mat->Diffuse.g;
    d3dMat.Diffuse.b = mat->Diffuse.b;
    d3dMat.Diffuse.a = mat->Diffuse.a;

    d3dMat.Ambient.r = mat->Ambient.r;
    d3dMat.Ambient.g = mat->Ambient.g;
    d3dMat.Ambient.b = mat->Ambient.b;
    d3dMat.Ambient.a = mat->Ambient.a;

    d3dMat.Specular.r = mat->Specular.r;
    d3dMat.Specular.g = mat->Specular.g;
    d3dMat.Specular.b = mat->Specular.b;
    d3dMat.Specular.a = mat->Specular.a;

    d3dMat.Emissive.r = mat->Emissive.r;
    d3dMat.Emissive.g = mat->Emissive.g;
    d3dMat.Emissive.b = mat->Emissive.b;
    d3dMat.Emissive.a = mat->Emissive.a;

    d3dMat.Power = mat->SpecularPower;

    return SUCCEEDED(m_Device->SetMaterial(&d3dMat));
}

CKBOOL CKDX9RasterizerContext::SetViewport(CKViewportData *data)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_Device || !data)
        return FALSE;

    m_ViewportData = *data;

    D3DVIEWPORT9 d3dViewport;
    d3dViewport.X = data->ViewX;
    d3dViewport.Y = data->ViewY;
    d3dViewport.Width = data->ViewWidth;
    d3dViewport.Height = data->ViewHeight;
    d3dViewport.MinZ = data->ViewZMin;
    d3dViewport.MaxZ = data->ViewZMax;

    // Validate viewport dimensions
    if (d3dViewport.Width == 0 || d3dViewport.Height == 0)
    {
        // Use default size if invalid dimensions provided
        d3dViewport.Width = m_Width;
        d3dViewport.Height = m_Height;
    }

    // Ensure Z range is valid
    if (d3dViewport.MinZ > d3dViewport.MaxZ)
    {
        float temp = d3dViewport.MinZ;
        d3dViewport.MinZ = d3dViewport.MaxZ;
        d3dViewport.MaxZ = temp;
    }

    return SUCCEEDED(m_Device->SetViewport(&d3dViewport));
}

CKBOOL CKDX9RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    if (!m_Device)
        return FALSE;

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
            m_MatrixUptodate = 0; // Clear all flags to force recomputation
            type = D3DTS_VIEW;
            break;

        case VXMATRIX_PROJECTION:
            m_ProjectionMatrix = Mat;
            matrixMask = PROJ_TRANSFORM;
            m_MatrixUptodate = 0; // Clear all flags to force recomputation
            type = D3DTS_PROJECTION;
            break;

        case VXMATRIX_TEXTURE0:
            matrixMask = TEXTURE0_TRANSFORM;
            type = D3DTS_TEXTURE0;
            break;

        case VXMATRIX_TEXTURE1:
        case VXMATRIX_TEXTURE2:
        case VXMATRIX_TEXTURE3:
        case VXMATRIX_TEXTURE4:
        case VXMATRIX_TEXTURE5:
        case VXMATRIX_TEXTURE6:
        case VXMATRIX_TEXTURE7:
            matrixMask = TEXTURE0_TRANSFORM << (Type - VXMATRIX_TEXTURE0);
            type = (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + (Type - VXMATRIX_TEXTURE0));
            break;

        default:
            return FALSE;
    }

    if (VxMatrix::Identity() == Mat)
    {
        // If already set as identity, return success
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
    if (!m_Device)
        return FALSE;

    if (State >= VXRENDERSTATE_MAXSTATE)
        return FALSE;

    // Check if this state is locked or disabled
    if (m_StateCache[State].Flags != 0)
        return TRUE;

    // Cache hit check
    if (m_StateCache[State].Valid && m_StateCache[State].Value == Value)
    {
        ++m_RenderStateCacheHit;
        return TRUE;
    }

    // Cache miss
    ++m_RenderStateCacheMiss;
    m_StateCache[State].Value = Value;
    m_StateCache[State].Valid = TRUE;

    // Check if this state is in the miss mask (states to ignore)
    if (m_StateCacheMissMask.IsSet(State))
        return FALSE;

    // Check if this state is in the hit mask (states with special handling)
    if (m_StateCacheHitMask.IsSet(State))
    {
        static const D3DCULL VXCullModes[4] = {D3DCULL_NONE, D3DCULL_NONE, D3DCULL_CW, D3DCULL_CCW};
        static const D3DCULL VXCullModesInverted[4] = {D3DCULL_NONE, D3DCULL_NONE, D3DCULL_CCW, D3DCULL_CW};

        if (State == VXRENDERSTATE_CULLMODE)
        {
            if (Value >= 4)
                Value = 0; // Default to VXCULL_NONE
            return SUCCEEDED(m_Device->SetRenderState(D3DRS_CULLMODE, m_InverseWinding ? VXCullModesInverted[Value] : VXCullModes[Value]));
        }
        if (State == VXRENDERSTATE_INVERSEWINDING)
        {
            m_InverseWinding = Value != 0;
            InvalidateStateCache(VXRENDERSTATE_CULLMODE);
        }
        return TRUE;
    }

    // Set the actual state in D3D (using translated values if needed)
    // CKDWORD translatedValue = Value;
    // if (m_TranslatedRenderStates[State])
    //     translatedValue = m_TranslatedRenderStates[State][Value];

    return SUCCEEDED(m_Device->SetRenderState((D3DRENDERSTATETYPE)State, Value));
}

CKBOOL CKDX9RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    if (!Value || State >= VXRENDERSTATE_MAXSTATE)
        return FALSE;

    // If the cache is valid, return the cached value
    if (m_StateCache[State].Valid)
    {
        *Value = m_StateCache[State].Value;
        return TRUE;
    }

    // Special handling for INVERSEWINDING which is handled internally
    if (State == VXRENDERSTATE_INVERSEWINDING)
    {
        *Value = m_InverseWinding;
        return TRUE;
    }

    // For states that are in the StateCacheHitMask but don't have special handling in SetRenderState,
    // we need to add them here if needed

    // Return default value for any other state
    *Value = m_StateCache[State].DefaultValue;
    return FALSE;
}

CKBOOL CKDX9RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
#if LOGGING && LOG_SETTEXTURE
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
    if (Stage < 0 || Stage >= m_Driver->m_3DCaps.MaxNumberTextureStage)
        return FALSE;

    if (!m_Device)
        return FALSE;

    HRESULT hr = S_OK;

    // Case 1: Disabling a texture or using an invalid texture
    if (Texture == 0 || Texture >= m_Textures.Size() || m_Textures[Texture] == NULL ||
        !(m_Textures[Texture]->Flags & CKRST_TEXTURE_VALID))
    {
        // Disable texture for this stage
        hr = m_Device->SetTexture(Stage, NULL);

        // Stage 0 needs special default settings when texture is removed
        if (SUCCEEDED(hr) && Stage == 0)
        {
            // Disable texturing, use diffuse color
            hr = m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            if (SUCCEEDED(hr))
                hr = m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

            if (SUCCEEDED(hr))
                hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

            if (SUCCEEDED(hr))
                hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        }

        return SUCCEEDED(hr);
    }

    // Case 2: Setting a valid texture
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);

    // Check if we have a valid texture surface
    if (!desc->DxTexture)
        return FALSE;

    // Set the texture based on its type
    if (desc->Flags & CKRST_TEXTURE_CUBEMAP)
    {
        hr = m_Device->SetTexture(Stage, desc->DxCubeTexture);
    }
    else if (desc->Flags & CKRST_TEXTURE_VOLUMEMAP)
    {
        hr = m_Device->SetTexture(Stage, desc->DxVolumeTexture);
    }
    else
    {
        hr = m_Device->SetTexture(Stage, desc->DxTexture);
    }

    if (FAILED(hr))
        return FALSE;

    // Default stage setup for stage 0 (modulation which is most common)
    if (Stage == 0)
    {
        // Default to modulation (texture * vertex color)
        hr = m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        if (FAILED(hr))
            return FALSE;

        hr = m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        if (FAILED(hr))
            return FALSE;

        hr = m_Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
        if (FAILED(hr))
            return FALSE;

        hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        if (FAILED(hr))
            return FALSE;

        hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        if (FAILED(hr))
            return FALSE;

        hr = m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        if (FAILED(hr))
            return FALSE;
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_Device || Stage < 0 || Stage >= m_Driver->m_3DCaps.MaxNumberTextureStage)
        return FALSE;

    HRESULT hr = S_OK;

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
                    hr = block->Apply();
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
                    hr = block->Apply();
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
                    hr = block->Apply();
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Handle the case of setting null shader (disabling programmable pipeline)
    if (VShaderIndex == 0)
    {
        m_CurrentVertexShaderCache = 0;
        m_CurrentVertexFormatCache = 0;
        return SUCCEEDED(m_Device->SetVertexShader(NULL));
    }

    if (VShaderIndex >= m_VertexShaders.Size())
        return FALSE;

    // Get and validate shader descriptor
    CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(m_VertexShaders[VShaderIndex]);
    if (!desc || !desc->DxShader)
        return FALSE;

    // Cache current shader for optimizing redundant calls
    if (m_CurrentVertexShaderCache == VShaderIndex)
        return TRUE; // Already set

    // Update caches
    m_CurrentVertexShaderCache = VShaderIndex;
    m_CurrentVertexFormatCache = 0;

    // Actually set the shader on the device
    return SUCCEEDED(m_Device->SetVertexShader(desc->DxShader));
}

CKBOOL CKDX9RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Handle the case of setting null shader (disabling programmable pipeline)
    if (PShaderIndex == 0)
    {
        if (m_CurrentPixelShaderCache == 0)
            return TRUE; // Already disabled

        m_CurrentPixelShaderCache = 0;
        return SUCCEEDED(m_Device->SetPixelShader(NULL));
    }

    if (PShaderIndex >= m_PixelShaders.Size())
        return FALSE;

    // Get and validate shader
    CKDX9PixelShaderDesc *desc = static_cast<CKDX9PixelShaderDesc *>(m_PixelShaders[PShaderIndex]);
    if (!desc || !desc->DxShader)
        return FALSE;

    // Cache current shader for optimizing redundant calls
    if (m_CurrentPixelShaderCache == PShaderIndex)
        return TRUE; // Already set

    // Update cache and set shader
    m_CurrentPixelShaderCache = PShaderIndex;
    return SUCCEEDED(m_Device->SetPixelShader(desc->DxShader));
}

CKBOOL CKDX9RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    if (!Data || CstCount == 0)
        return FALSE;

    // Check register bounds (typical DX9 limit is 256 constants)
    if (Register + CstCount > 256)
        return FALSE;

    // Safe casting - ensure alignment requirement for float data
    if ((reinterpret_cast<uintptr_t>(Data) & 0x3) != 0)
    {
        // Data is not properly aligned for float
        // Consider creating an aligned copy before setting
        float *alignedData = new float[CstCount * 4];
        memcpy(alignedData, Data, CstCount * 4 * sizeof(float));
        HRESULT hr = m_Device->SetVertexShaderConstantF(Register, alignedData, CstCount);
        delete[] alignedData;
        return SUCCEEDED(hr);
    }

    return SUCCEEDED(m_Device->SetVertexShaderConstantF(Register, static_cast<const float *>(Data), CstCount));
}

CKBOOL CKDX9RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    if (!Data || CstCount == 0)
        return FALSE;

    // Check register bounds (typical DX9 limit is 224 constants for PS)
    if (Register + CstCount > 224)
        return FALSE;

    // Safe casting - ensure alignment requirement for float data
    if ((reinterpret_cast<uintptr_t>(Data) & 0x3) != 0)
    {
        // Data is not properly aligned for float
        // Consider creating an aligned copy before setting
        float *alignedData = new float[CstCount * 4];
        memcpy(alignedData, Data, CstCount * 4 * sizeof(float));
        HRESULT hr = m_Device->SetPixelShaderConstantF(Register, alignedData, CstCount);
        delete[] alignedData;
        return SUCCEEDED(hr);
    }

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

    if (!data || data->VertexCount <= 0)
        return FALSE;

    if (!m_SceneBegined && !BeginScene())
        return FALSE;

    // Calculate vertex format and size
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);

    // Set clipping state based on flags
    CKBOOL clip = FALSE;
    if (data->Flags & CKRST_DP_DOCLIP)
    {
        if (!SetRenderState(VXRENDERSTATE_CLIPPING, TRUE))
            return FALSE;
        clip = TRUE;
    }
    else
    {
        if (!SetRenderState(VXRENDERSTATE_CLIPPING, FALSE))
            return FALSE;
    }

    // Get a suitable dynamic vertex buffer
    CKDWORD index = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    CKDX9VertexBufferDesc *vertexBufferDesc = (index < m_VertexBuffers.Size()) ? static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[index]) : NULL;
    if (!vertexBufferDesc || !vertexBufferDesc->DxBuffer)
        return FALSE;

    // Lock the vertex buffer with proper strategy
    void *ppbData = NULL;
    HRESULT hr = E_FAIL;
    CKDWORD startIndex = 0;
    DWORD lockFlags = 0;
    CKBOOL appending = FALSE;

    // Try to append to existing buffer if there's room
    if (vertexBufferDesc->m_CurrentVCount + data->VertexCount <= vertexBufferDesc->m_MaxVertexCount)
    {
        hr = vertexBufferDesc->DxBuffer->Lock(vertexSize * vertexBufferDesc->m_CurrentVCount, vertexSize * data->VertexCount, &ppbData, D3DLOCK_NOOVERWRITE);
        if (SUCCEEDED(hr) && ppbData)
        {
            startIndex = vertexBufferDesc->m_CurrentVCount;
            vertexBufferDesc->m_CurrentVCount += data->VertexCount;
            appending = TRUE;
        }
    }

    // If the append failed or there's not enough space, discard and start fresh
    if (!appending)
    {
        hr = vertexBufferDesc->DxBuffer->Lock(0, vertexSize * data->VertexCount, &ppbData, D3DLOCK_DISCARD);
        if (SUCCEEDED(hr) && ppbData)
        {
            startIndex = 0;
            vertexBufferDesc->m_CurrentVCount = data->VertexCount;
        }
    }

    // If all locking attempts failed, return error
    if (FAILED(hr) || !ppbData)
        return FALSE;

    // Copy vertex data to the buffer
    CKRSTLoadVertexBuffer(reinterpret_cast<CKBYTE *>(ppbData), vertexFormat, vertexSize, data);

    // Unlock the buffer
    hr = vertexBufferDesc->DxBuffer->Unlock();
    if (FAILED(hr))
        return FALSE;

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

    if (VertexCount == 0)
        return FALSE;

    if (VertexBuffer >= m_VertexBuffers.Size())
        return FALSE;

    CKVertexBufferDesc *vertexBufferDesc = m_VertexBuffers[VertexBuffer];
    if (!vertexBufferDesc || !(vertexBufferDesc->m_Flags & CKRST_VB_VALID))
        return FALSE;

    if (indices && indexcount <= 0)
        return FALSE;

    // Calculate total vertices needed and ensure buffer has enough
    if (StartIndex + VertexCount > vertexBufferDesc->m_MaxVertexCount)
        return FALSE;

    // Begin the scene if needed
    if (!m_SceneBegined && !BeginScene())
        return FALSE;

    // Cast buffer to DX9-specific implementation
    CKDX9VertexBufferDesc *dxVertexBufferDesc = static_cast<CKDX9VertexBufferDesc *>(vertexBufferDesc);
    if (!dxVertexBufferDesc->DxBuffer)
        return FALSE;

    // Draw the primitive
    return InternalDrawPrimitiveVB(pType, dxVertexBufferDesc, StartIndex, VertexCount, indices, indexcount, TRUE);
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

    if (VertexCount == 0 || Indexcount <= 0)
        return FALSE;

    // Check if indices are valid
    if (VB >= m_VertexBuffers.Size() || IB >= m_IndexBuffers.Size())
        return FALSE;

    // Get and validate both buffers
    CKDX9VertexBufferDesc *vertexBufferDesc = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vertexBufferDesc || !(vertexBufferDesc->m_Flags & CKRST_VB_VALID) || !vertexBufferDesc->DxBuffer)
        return FALSE;

    CKDX9IndexBufferDesc *indexBufferDesc = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!indexBufferDesc || !(indexBufferDesc->m_Flags & CKRST_VB_VALID) || !indexBufferDesc->DxBuffer)
        return FALSE;

    // Validate buffer sizes
    if (MinVIndex + VertexCount > vertexBufferDesc->m_MaxVertexCount)
        return FALSE;

    if (StartIndex + Indexcount > indexBufferDesc->m_MaxIndexCount)
        return FALSE;

    // Begin scene if needed
    if (!m_SceneBegined && !BeginScene())
        return FALSE;

    SetupStreams(vertexBufferDesc->DxBuffer, vertexBufferDesc->m_VertexFormat, 0, vertexBufferDesc->m_VertexSize);

    // Calculate primitive count based on primitive type
    int primitiveCount = Indexcount;
    switch (pType)
    {
        case VX_LINELIST:
            primitiveCount = primitiveCount / 2;
            break;
        case VX_LINESTRIP:
            primitiveCount = primitiveCount - 1;
            break;
        case VX_TRIANGLELIST:
            primitiveCount = primitiveCount / 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            primitiveCount = primitiveCount - 2;
            break;
        default:
            break;
    }

    // Make sure we have valid primitive count
    if (primitiveCount <= 0)
        return FALSE;

    // Set index buffer
    HRESULT hr = m_Device->SetIndices(indexBufferDesc->DxBuffer);
    if (FAILED(hr))
        return FALSE;

    // Draw the primitive
    hr = m_Device->DrawIndexedPrimitive(
        (D3DPRIMITIVETYPE)pType,
        MinVIndex,    // Base vertex index offset
        0,            // Minimum vertex index
        VertexCount,  // Number of vertices
        StartIndex,   // Start index in index buffer
        primitiveCount // Number of primitives
    );

    return SUCCEEDED(hr);
}

CKBOOL CKDX9RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
                for (CKSPRTextInfo *it = desc->Textures.Begin(); it != desc->Textures.End(); ++it)
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

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

    if (desc->Format.BytesPerLine == 0)
        desc->Format.BytesPerLine = lockRect.Pitch;

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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!m_Device)
        return FALSE;

    // End any current scene
    EndScene();

    // Restoring the default render target if TextureObject is 0
    if (TextureObject == 0)
    {
        if (!m_DefaultBackBuffer)
            return FALSE;
        
        HRESULT hr = m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
        if (FAILED(hr))
            return FALSE;

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
                m_CurrentTextureIndex = 0;
            }
        }

        SAFERELEASE(m_DefaultBackBuffer);
        SAFERELEASE(m_DefaultDepthBuffer);
        return TRUE;
    }

    if (TextureObject >= m_Textures.Size() || m_DefaultBackBuffer)
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
    {
        if (m_Textures[TextureObject] == desc && m_Textures[TextureObject] != NULL)
        {
            delete desc;
            m_Textures[TextureObject] = NULL;
        }
        SAFERELEASE(m_DefaultBackBuffer);
        return FALSE;
    }

    hr = m_Device->GetDepthStencilSurface(&m_DefaultDepthBuffer);
    if (FAILED(hr))
    {
        // No depth buffer is ok, but we should clear the pointer
        SAFERELEASE(m_DefaultDepthBuffer);
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
                    else
                    {
                        SAFERELEASE(surface);
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
                else
                {
                    SAFERELEASE(surface);
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
    if (FAILED(hr))
    {
        // Failed to set render target, clean up
        desc->Flags &= ~CKRST_TEXTURE_VALID;
        SAFERELEASE(surface);

        if (!surfaceSuccess)
        {
            if (cubemap)
            {
                SAFERELEASE(desc->DxCubeTexture);
            }
            else
            {
                SAFERELEASE(desc->DxTexture);
                SAFERELEASE(desc->DxRenderTexture);
            }
        }

        SAFERELEASE(m_DefaultBackBuffer);
        SAFERELEASE(m_DefaultDepthBuffer);
        return FALSE;
    }

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
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!src || !dst || Sprite >= m_Sprites.Size())
        return FALSE;

    CKSpriteDesc *sprite = m_Sprites[Sprite];
    if (!sprite || sprite->Textures.IsEmpty())
        return FALSE;

    // Basic boundary checks with early exit
    if (src->GetWidth() <= 0.0f || src->GetHeight() <= 0.0f ||
        dst->GetWidth() <= 0.0f || dst->GetHeight() <= 0.0f ||
        src->right < 0.0f || src->bottom < 0.0f ||
        sprite->Format.Width <= src->left || sprite->Format.Height <= src->top ||
        dst->right < 0.0f || dst->bottom < 0.0f ||
        m_Width <= dst->left || m_Height <= dst->top)
        return FALSE;

    // Begin scene if needed
    if (!m_SceneBegined && !BeginScene())
        return FALSE;

    // Save render states to restore later
    CKDWORD oldZEnable, oldZWriteEnable, oldLighting, oldCullMode, oldClipping, oldAlphaBlend, oldSrcBlend, oldDestBlend;
    GetRenderState(VXRENDERSTATE_ZENABLE, &oldZEnable);
    GetRenderState(VXRENDERSTATE_ZWRITEENABLE, &oldZWriteEnable);
    GetRenderState(VXRENDERSTATE_LIGHTING, &oldLighting);
    GetRenderState(VXRENDERSTATE_CULLMODE, &oldCullMode);
    GetRenderState(VXRENDERSTATE_CLIPPING, &oldClipping);
    GetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, &oldAlphaBlend);
    GetRenderState(VXRENDERSTATE_SRCBLEND, &oldSrcBlend);
    GetRenderState(VXRENDERSTATE_DESTBLEND, &oldDestBlend);

    // Save original viewport
    D3DVIEWPORT9 oldViewport;
    m_Device->GetViewport(&oldViewport);

    // Set sprite rendering states once
    SetRenderState(VXRENDERSTATE_ZENABLE, FALSE);
    SetRenderState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    SetRenderState(VXRENDERSTATE_CLIPPING, FALSE);
    SetRenderState(VXRENDERSTATE_TEXTUREPERSPECTIVE, FALSE);
    SetRenderState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
    SetRenderState(VXRENDERSTATE_SRCBLEND, VXBLEND_SRCALPHA);
    SetRenderState(VXRENDERSTATE_DESTBLEND, VXBLEND_INVSRCALPHA);

    // Set sprite viewport
    D3DVIEWPORT9 viewport = {0, 0, m_Width, m_Height, 0.0f, 1.0f};
    m_Device->SetViewport(&viewport);

    // Set up texture stage states once
    SetTextureStageState(0, CKRST_TSS_TEXTUREMAPBLEND, VXTEXTUREBLEND_COPY);
    SetTextureStageState(0, CKRST_TSS_MINFILTER, VXTEXTUREFILTER_NEAREST);
    SetTextureStageState(0, CKRST_TSS_MAGFILTER, VXTEXTUREFILTER_NEAREST);
    SetTextureStageState(0, CKRST_TSS_ADDRESSU, VXTEXTURE_ADDRESSCLAMP);
    SetTextureStageState(0, CKRST_TSS_ADDRESSV, VXTEXTURE_ADDRESSCLAMP);

    // Calculate scaling ratios
    const float widthRatio = dst->GetWidth() / src->GetWidth();
    const float heightRatio = dst->GetHeight() / src->GetHeight();

    // ----- BATCH RENDERING IMPLEMENTATION -----

    // Step 1: Define a structure to hold fragment data
    struct SpriteFragment
    {
        CKDWORD TextureId;
        float ScreenLeft, ScreenTop, ScreenRight, ScreenBottom;
        float TexU1, TexV1, TexU2, TexV2;
    };

    // Step 2: Pre-process all visible fragments to collect and sort them
    XArray<SpriteFragment> visibleFragments;

    // First pass: Collect visible fragments
    for (XArray<CKSPRTextInfo>::Iterator it = sprite->Textures.Begin(); it != sprite->Textures.End(); ++it)
    {
        const float tx = it->x;
        const float ty = it->y;
        const float tw = it->w;
        const float th = it->h;
        const float tr = tx + tw;
        const float tb = ty + th;

        // Skip textures completely outside view
        if (tx > src->right || ty > src->bottom || tr < src->left || tb < src->top)
            continue;

        // Create fragment for this texture piece
        SpriteFragment fragment;
        fragment.TextureId = it->IndexTexture;

        // Default full texture coordinates
        fragment.TexU1 = 0.0f;
        fragment.TexV1 = 0.0f;
        fragment.TexU2 = static_cast<float>(tw) / it->sw;
        fragment.TexV2 = static_cast<float>(th) / it->sh;

        // Default screen coordinates
        fragment.ScreenLeft = (tx - src->left) * widthRatio + dst->left;
        fragment.ScreenTop = (ty - src->top) * heightRatio + dst->top;
        fragment.ScreenRight = (tr - src->left) * widthRatio + dst->left;
        fragment.ScreenBottom = (tb - src->top) * heightRatio + dst->top;

        // Handle clipping on right/bottom
        if (src->right < tr)
        {
            const float clippedWidth = src->right - tx;
            fragment.ScreenRight = (tx + clippedWidth - src->left) * widthRatio + dst->left;
            fragment.TexU2 = clippedWidth / it->sw;
        }

        if (src->bottom < tb)
        {
            const float clippedHeight = src->bottom - ty;
            fragment.ScreenBottom = (ty + clippedHeight - src->top) * heightRatio + dst->top;
            fragment.TexV2 = clippedHeight / it->sh;
        }

        // Handle clipping on left/top
        if (src->left > tx)
        {
            const float clippedLeft = src->left - tx;
            fragment.TexU1 = clippedLeft / it->sw;
            fragment.ScreenLeft = dst->left;
        }

        if (src->top > ty)
        {
            const float clippedTop = src->top - ty;
            fragment.TexV1 = clippedTop / it->sh;
            fragment.ScreenTop = dst->top;
        }

        // Add to visible fragments list
        visibleFragments.PushBack(fragment);
    }

    // Return if nothing to render
    if (visibleFragments.IsEmpty())
    {
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        SetRenderState(VXRENDERSTATE_CLIPPING, oldClipping);
        SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
        SetRenderState(VXRENDERSTATE_SRCBLEND, oldSrcBlend);
        SetRenderState(VXRENDERSTATE_DESTBLEND, oldDestBlend);
        return TRUE;
    }

    // Step 3: Sort fragments by texture ID to minimize texture switches
    // Simple insertion sort (efficient for small arrays)
    for (int i = 1; i < visibleFragments.Size(); ++i)
    {
        SpriteFragment key = visibleFragments[i];
        int j = i - 1;

        while (j >= 0 && visibleFragments[j].TextureId > key.TextureId)
        {
            visibleFragments[j + 1] = visibleFragments[j];
            --j;
        }

        visibleFragments[j + 1] = key;
    }

    // Step 4: Allocate a vertex buffer for the entire sprite
    const int totalVertices = visibleFragments.Size() * 4; // 4 vertices per quad

    CKDWORD vertexBufferIndex = GetDynamicVertexBuffer(CKRST_VF_TLVERTEX, totalVertices, sizeof(CKVertex), 1);
    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[vertexBufferIndex]);
    if (!vb || !vb->DxBuffer)
    {
        // Clean up and return failure
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        SetRenderState(VXRENDERSTATE_CLIPPING, oldClipping);
        SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
        SetRenderState(VXRENDERSTATE_SRCBLEND, oldSrcBlend);
        SetRenderState(VXRENDERSTATE_DESTBLEND, oldDestBlend);
        return FALSE;
    }

    // Step 5: Lock vertex buffer and fill with quad data
    void *pVertices = nullptr;
    CKDWORD startVertex = 0;
    HRESULT hr;

    // Try to append to existing buffer if there's room
    if (vb->m_CurrentVCount + totalVertices <= vb->m_MaxVertexCount)
    {
        hr = vb->DxBuffer->Lock(
            sizeof(CKVertex) * vb->m_CurrentVCount,
            sizeof(CKVertex) * totalVertices,
            &pVertices,
            D3DLOCK_NOOVERWRITE
        );

        if (SUCCEEDED(hr) && pVertices)
        {
            startVertex = vb->m_CurrentVCount;
            vb->m_CurrentVCount += totalVertices;
        }
    }

    // If append failed, discard and start fresh
    if (!pVertices)
    {
        hr = vb->DxBuffer->Lock(
            0,
            sizeof(CKVertex) * totalVertices,
            &pVertices,
            D3DLOCK_DISCARD
        );
        if (SUCCEEDED(hr) && pVertices)
        {
            startVertex = 0;
            vb->m_CurrentVCount = totalVertices;
        }
    }

    // If all lock attempts failed, clean up and return
    if (!pVertices)
    {
        m_Device->SetViewport(&oldViewport);
        SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
        SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
        SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
        SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
        SetRenderState(VXRENDERSTATE_CLIPPING, oldClipping);
        SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
        SetRenderState(VXRENDERSTATE_SRCBLEND, oldSrcBlend);
        SetRenderState(VXRENDERSTATE_DESTBLEND, oldDestBlend);
        return FALSE;
    }

    // Fill vertex buffer with all quads
    CKVertex *vertices = static_cast<CKVertex *>(pVertices);
    const CKDWORD diffuseColor = (R_MASK | G_MASK | B_MASK | A_MASK);

    for (int i = 0; i < visibleFragments.Size(); ++i)
    {
        const SpriteFragment &fragment = visibleFragments[i];
        CKVertex *quad = &vertices[i * 4];

        // Set common vertex attributes
        for (int v = 0; v < 4; ++v)
        {
            quad[v].Diffuse = diffuseColor;
            quad[v].Specular = A_MASK;
        }

        // Top-left vertex
        quad[0].V = VxVector4(fragment.ScreenLeft, fragment.ScreenTop, 0.0f, 1.0f);
        quad[0].tu = fragment.TexU1;
        quad[0].tv = fragment.TexV1;

        // Bottom-left vertex
        quad[1].V = VxVector4(fragment.ScreenLeft, fragment.ScreenBottom, 0.0f, 1.0f);
        quad[1].tu = fragment.TexU1;
        quad[1].tv = fragment.TexV2;

        // Bottom-right vertex
        quad[2].V = VxVector4(fragment.ScreenRight, fragment.ScreenBottom, 0.0f, 1.0f);
        quad[2].tu = fragment.TexU2;
        quad[2].tv = fragment.TexV2;

        // Top-right vertex
        quad[3].V = VxVector4(fragment.ScreenRight, fragment.ScreenTop, 0.0f, 1.0f);
        quad[3].tu = fragment.TexU2;
        quad[3].tv = fragment.TexV1;
    }

    // Unlock the vertex buffer
    vb->DxBuffer->Unlock();

    // Step 6: Setup vertex streams
    SetupStreams(vb->DxBuffer, CKRST_VF_TLVERTEX, 0, sizeof(CKVertex));

    // Step a7: Render batches (groups of fragments with same texture)
    CKDWORD currentTextureId = 0xFFFFFFFF;
    int batchStart = 0;

    // Process all fragments, detecting texture changes
    for (int i = 0; i <= visibleFragments.Size(); ++i)
    {
        // Process a batch when texture changes or at the end of all fragments
        if (i == visibleFragments.Size() || visibleFragments[i].TextureId != currentTextureId)
        {
            // Render current batch if it exists
            if (i > batchStart)
            {
                // Get texture
                CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[currentTextureId]);
                if (desc && desc->DxTexture)
                {
                    // Set the texture
                    m_Device->SetTexture(0, desc->DxTexture);

                    // Draw all quads in this batch
                    for (int j = batchStart; j < i; ++j)
                    {
                        m_Device->DrawPrimitive(D3DPT_TRIANGLEFAN, startVertex + (j * 4), 2);
                    }
                }
            }

            // Start new batch if not at the end
            if (i < visibleFragments.Size())
            {
                currentTextureId = visibleFragments[i].TextureId;
                batchStart = i;
            }
        }
    }

    // Step 8: Cleanup and restore state
    m_Device->SetTexture(0, NULL);
    m_Device->SetStreamSource(0, NULL, 0, 0);
    m_Device->SetViewport(&oldViewport);

    // Restore render states
    SetRenderState(VXRENDERSTATE_ZENABLE, oldZEnable);
    SetRenderState(VXRENDERSTATE_ZWRITEENABLE, oldZWriteEnable);
    SetRenderState(VXRENDERSTATE_LIGHTING, oldLighting);
    SetRenderState(VXRENDERSTATE_CULLMODE, oldCullMode);
    SetRenderState(VXRENDERSTATE_CLIPPING, oldClipping);
    SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
    SetRenderState(VXRENDERSTATE_SRCBLEND, oldSrcBlend);
    SetRenderState(VXRENDERSTATE_DESTBLEND, oldDestBlend);

    return TRUE;
}

void *CKDX9RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount, CKRST_LOCKFLAGS Lock)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (VB >= m_VertexBuffers.Size())
        return NULL;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return NULL;

    void *pVertices = NULL;
    if (FAILED(vb->DxBuffer->Lock(StartVertex * vb->m_VertexSize, VertexCount * vb->m_VertexSize, &pVertices, Lock << 12)))
        return NULL;

    return pVertices;
}

CKBOOL CKDX9RasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return FALSE;

    return SUCCEEDED(vb->DxBuffer->Unlock());
}

int CKDX9RasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
    UINT rowSize = min(img_desc.BytesPerLine, lockedRect.Pitch);

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
    RECT srcRect = {0, 0, min(img_desc.Width, destWidth), min(img_desc.Height, destHeight)};
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
    // If no change in state, return
    if (m_TransparentMode == Trans)
        return;
        
    // Update transparent mode flag
    m_TransparentMode = Trans;
    
    if (!Trans)
    {
        ResetDirtyRects();
    }
}

void CKDX9RasterizerContext::AddDirtyRect(CKRECT *Rect)
{
    if (!Rect)
    {
        // Mark entire screen as dirty
        m_CleanAllRects = TRUE;
    }
    else
    {
        // Don't add if we're already cleaning everything
        if (m_CleanAllRects)
            return;

        // Validate rectangle
        if (Rect->right <= Rect->left || Rect->bottom <= Rect->top)
            return;

        // Add to dirty rectangles list
        m_DirtyRects.PushBack(*Rect);

        // Limit number of rectangles to prevent performance issues
        if (m_DirtyRects.Size() > 64)
            m_CleanAllRects = TRUE;
    }
}

void CKDX9RasterizerContext::RestoreScreenBackup()
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Do nothing if not in transparent mode
    if (!m_TransparentMode)
        return;
        
    // Create screen backup if it doesn't exist
    if (!m_ScreenBackup)
    {
        HRESULT hr;
        IDirect3DSurface9* backBuffer = NULL;
        
        // Get the back buffer
        hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        if (FAILED(hr) || !backBuffer)
            return;
            
        // Get its description
        D3DSURFACE_DESC desc;
        hr = backBuffer->GetDesc(&desc);
        
        // Create a matching offscreen surface for backup
        if (SUCCEEDED(hr))
        {
            hr = m_Device->CreateOffscreenPlainSurface(
                desc.Width, 
                desc.Height,
                desc.Format,
                D3DPOOL_SYSTEMMEM,
                &m_ScreenBackup,
                NULL
            );
        }
        
        SAFERELEASE(backBuffer);
        
        // If we couldn't create the backup, exit
        if (!m_ScreenBackup)
            return;
    }
    
    // Get the current back buffer
    IDirect3DSurface9* backBuffer = NULL;
    HRESULT hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (FAILED(hr) || !backBuffer)
        return;
        
    // If we have specific dirty rectangles and not cleaning all, update only those
    if (!m_CleanAllRects && m_DirtyRects.Size() > 0)
    {
        for (XArray<CKRECT>::Iterator it = m_DirtyRects.Begin(); it != m_DirtyRects.End(); ++it)
        {
            RECT rect = { it->left, it->top, it->right, it->bottom };
            POINT destPoint = { rect.left, rect.top };
            
            // Copy the dirty rect from backup to back buffer
            hr = m_Device->UpdateSurface(m_ScreenBackup, &rect, backBuffer, &destPoint);
        }
    }
    else
    {
        // Copy the entire surface
        hr = m_Device->UpdateSurface(m_ScreenBackup, NULL, backBuffer, NULL);
    }
    
    // Release the back buffer
    SAFERELEASE(backBuffer);
    
    // Reset dirty rectangles
    ResetDirtyRects();
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (Texture >= m_Textures.Size())
        return FALSE;

    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxCubeTexture)
        return FALSE;

    // Skip loading render targets since they're managed differently
    if ((desc->Flags & CKRST_TEXTURE_RENDERTARGET) != 0)
        return TRUE;

    // Ensure this is actually a cube map texture
    if ((desc->Flags & CKRST_TEXTURE_CUBEMAP) == 0)
        return FALSE;

    // Determine the actual mipmap level to load
    int actualMipLevel = (miplevel < 0) ? 0 : miplevel;

    // Get surface description for the specified mipmap level
    D3DSURFACE_DESC surfaceDesc;
    HRESULT hr = desc->DxCubeTexture->GetLevelDesc(actualMipLevel, &surfaceDesc);
    if (FAILED(hr))
        return FALSE;


    // Path 1: DXTn compressed textures using D3DX helper functions
    if ((surfaceDesc.Format == D3DFMT_DXT1 ||
        surfaceDesc.Format == D3DFMT_DXT2 ||
        surfaceDesc.Format == D3DFMT_DXT3 ||
        surfaceDesc.Format == D3DFMT_DXT4 ||
        surfaceDesc.Format == D3DFMT_DXT5) &&
        (D3DXLoadSurfaceFromSurface && D3DXLoadSurfaceFromMemory))
    {
        // Get the cube map face surface
        IDirect3DSurface9 *pCubeMapSurface = NULL;
        hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, actualMipLevel, &pCubeMapSurface);
        if (FAILED(hr) || !pCubeMapSurface)
            return FALSE;

        // Set up source rectangle with correct dimensions
        RECT srcRect = {0, 0, (LONG)SurfDesc.Width, (LONG)SurfDesc.Height};

        // Convert source format
        VX_PIXELFORMAT vxpf = VxImageDesc2PixelFormat(SurfDesc);
        D3DFORMAT format = VxPixelFormatToD3DFormat(vxpf);

        // Load data from memory to surface
        hr = D3DXLoadSurfaceFromMemory(pCubeMapSurface, NULL, NULL, SurfDesc.Image, format, SurfDesc.BytesPerLine, NULL, &srcRect, D3DX_FILTER_LINEAR, 0);
        if (FAILED(hr))
        {
            SAFERELEASE(pCubeMapSurface);
            return FALSE;
        }

        // Generate mipmaps if needed
        CKDWORD mipMapCount = m_Textures[Texture]->MipMapCount;
        if (miplevel == -1 && mipMapCount > 0)
        {
            // Create all mipmap levels from the loaded surface
            for (int i = 1; i < mipMapCount + 1; ++i)
            {
                IDirect3DSurface9 *pMipSurface = NULL;
                hr = desc->DxCubeTexture->GetCubeMapSurface((D3DCUBEMAP_FACES)Face, i, &pMipSurface);
                if (FAILED(hr) || !pMipSurface)
                {
                    SAFERELEASE(pCubeMapSurface);
                    return FALSE;
                }

                // Create mipmap using box filter
                hr = D3DXLoadSurfaceFromSurface(pMipSurface, NULL, NULL, pCubeMapSurface, NULL, NULL, D3DX_FILTER_BOX, 0);

                // Clean up mip surface regardless of result
                SAFERELEASE(pMipSurface);

                if (FAILED(hr))
                {
                    SAFERELEASE(pCubeMapSurface);
                    return FALSE;
                }
            }
        }

        // Clean up and return
        SAFERELEASE(pCubeMapSurface);
        return TRUE;
    }

    // Path 2: Manual texture loading and mipmap generation
    // Create a copy of the source image data
    VxImageDescEx src = SurfDesc;
    VxImageDescEx dst;
    CKBYTE *image = NULL;
    CKBOOL needsCleanup = FALSE;

    // Generate mipmaps if requested
    CKBOOL generateMipmaps = (miplevel == -1 && desc->MipMapCount > 0);

    // Allocate temporary buffer for mipmap generation if needed
    if (generateMipmaps)
    {
        // Allocate memory for image processing
        image = m_Owner->AllocateObjects(surfaceDesc.Width * surfaceDesc.Height);
        if (!image)
            return FALSE;

        needsCleanup = TRUE;

        // If source dimensions don't match texture dimensions, resize
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

            // Blit the source image to our working buffer
            VxDoBlit(src, dst);
            src = dst;
        }
    }

    // Lock and load base level
    D3DLOCKED_RECT lockRect;
    hr = desc->DxCubeTexture->LockRect((D3DCUBEMAP_FACES)Face, actualMipLevel, &lockRect, NULL, 0);
    if (FAILED(hr))
    {
        // Clean up on failure
        return FALSE;
    }

    // Copy data to texture
    LoadSurface(surfaceDesc, lockRect, src);

    // Unlock base level
    hr = desc->DxCubeTexture->UnlockRect((D3DCUBEMAP_FACES)Face, actualMipLevel);
    if (FAILED(hr))
    {
        return FALSE;
    }

    // Generate and load mipmaps if needed
    if (generateMipmaps && image)
    {
        dst = src;

        // For each mipmap level
        for (int i = 1; i < desc->MipMapCount + 1; ++i)
        {
            // Generate next mipmap level
            VxGenerateMipMap(dst, image);

            // Halve dimensions for next mipmap
            if (dst.Width > 1)
                dst.Width >>= 1;
            if (dst.Height > 1)
                dst.Height >>= 1;

            // Update stride for new dimensions
            dst.BytesPerLine = 4 * dst.Width;
            dst.Image = image;

            // Get destination surface description
            hr = desc->DxCubeTexture->GetLevelDesc(i, &surfaceDesc);
            if (FAILED(hr))
                return TRUE; // Return success for levels we managed to create

            // Lock the mipmap level
            hr = desc->DxCubeTexture->LockRect((D3DCUBEMAP_FACES)Face, i, &lockRect, NULL, 0);
            if (FAILED(hr))
                return TRUE; // Return success for levels we managed to create

            // Copy data to mipmap level
            LoadSurface(surfaceDesc, lockRect, dst);

            // Unlock the mipmap level
            hr = desc->DxCubeTexture->UnlockRect((D3DCUBEMAP_FACES)Face, i);
            if (FAILED(hr))
                return TRUE; // Return success for levels we managed to create

            // Stop if we've reached 1x1 texture
            if (dst.Width <= 1 && dst.Height <= 1)
                break;
        }
    }

    return TRUE;
}

void *CKDX9RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (IB >= m_IndexBuffers.Size())
        return NULL;

    CKDX9IndexBufferDesc *ib = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ib || !ib->DxBuffer)
        return NULL;

    // Validate StartIndex and IndexCount
    if (StartIndex >= ib->m_MaxIndexCount)
        return NULL;

    if (IndexCount == 0 || StartIndex + IndexCount > ib->m_MaxIndexCount)
        IndexCount = ib->m_MaxIndexCount - StartIndex;

    // Convert CKRST_LOCKFLAGS to D3DLOCK flags
    DWORD d3dLockFlags = 0;
    switch (Lock)
    {
        case CKRST_LOCK_NOOVERWRITE:
            d3dLockFlags = D3DLOCK_NOOVERWRITE;
            break;
        case CKRST_LOCK_DISCARD:
            d3dLockFlags = D3DLOCK_DISCARD;
            break;
        default:
            d3dLockFlags = 0; // Default lock
            break;
    }

    // Calculate byte offset for 16-bit indices (2 bytes per index)
    UINT offsetInBytes = StartIndex * 2;
    UINT sizeInBytes = IndexCount * 2;

    // Lock the buffer
    void *pIndices = NULL;
    if (FAILED(ib->DxBuffer->Lock(offsetInBytes, sizeInBytes, &pIndices, d3dLockFlags)))
        return NULL;

    return pIndices;
}

CKBOOL CKDX9RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    CKDX9IndexBufferDesc *ib = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ib || !ib->DxBuffer)
        return FALSE;

    return SUCCEEDED(ib->DxBuffer->Unlock());
}

CKBOOL CKDX9RasterizerContext::CreateTextureFromFile(CKDWORD Texture, const char *Filename, TexFromFile *param)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!Filename || !param)
        return FALSE;
        
    if (Texture >= m_Textures.Size())
        return FALSE;

    // Check if D3DX function is available
    if (!D3DXCreateTextureFromFileExA)
        return FALSE;

    // Determine the texture format to use
    D3DFORMAT format = VxPixelFormatToD3DFormat(param->Format);
    if (format == D3DFMT_UNKNOWN)
        format = D3DFMT_A8R8G8B8;

    // Determine mipmap settings
    UINT mipLevels = (param->MipLevels == 0) ? 1 : param->MipLevels;

    // Determine filter settings based on mipmaps
    DWORD filter = D3DX_FILTER_LINEAR;
    DWORD mipFilter = (mipLevels > 1) ? D3DX_FILTER_BOX : D3DX_FILTER_NONE;

    // Determine usage and pool based on dynamic flag
    DWORD usage = 0;
    D3DPOOL pool = D3DPOOL_MANAGED;

    if (param->IsDynamic)
    {
        usage = D3DUSAGE_DYNAMIC;
        pool = D3DPOOL_DEFAULT;
    }

    // Create the texture
    IDirect3DTexture9 *pTexture = NULL;
    HRESULT hr = D3DXCreateTextureFromFileExA(
        m_Device,               // Device
        Filename,               // Source file
        D3DX_DEFAULT,           // Width (use file's width)
        D3DX_DEFAULT,           // Height (use file's height)
        mipLevels,              // Number of mipmap levels
        usage,                  // Usage flags
        format,                 // Pixel format
        pool,                   // Memory pool
        filter,                 // Filter for resizing if needed
        mipFilter,              // Mipmap filter
        param->ColorKey,        // Color key for transparency
        NULL,                   // Source info (don't need)
        NULL,                   // Palette (don't need)
        &pTexture               // Result
    );
    if (FAILED(hr) || !pTexture)
        return FALSE;

    // Get texture information
    D3DSURFACE_DESC surfaceDesc = {};
    hr = pTexture->GetLevelDesc(0, &surfaceDesc);
    if (FAILED(hr))
    {
        SAFERELEASE(pTexture);
        return FALSE;
    }

    // Remove existing texture if any
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (desc)
    {
        delete desc;
        desc = NULL;
    }

    // Create and initialize new texture descriptor
    desc = new CKDX9TextureDesc();
    if (!desc)
    {
        SAFERELEASE(pTexture);
        return FALSE;
    }

    // Set up the texture descriptor
    desc->DxTexture = pTexture;
    D3DFormatToTextureDesc(surfaceDesc.Format, desc);

    // Set appropriate flags
    CKDWORD flags = CKRST_TEXTURE_VALID | CKRST_TEXTURE_RGB;

    // Check if the texture has alpha
    D3DFORMAT actualFormat = surfaceDesc.Format;
    if (actualFormat == D3DFMT_A8R8G8B8 || 
        actualFormat == D3DFMT_A1R5G5B5 || 
        actualFormat == D3DFMT_A4R4G4B4 ||
        actualFormat == D3DFMT_A8B8G8R8 ||
        actualFormat == D3DFMT_A16B16G16R16 ||
        actualFormat == D3DFMT_A8P8 ||
        actualFormat == D3DFMT_A8L8 ||
        actualFormat == D3DFMT_A4L4 ||
        actualFormat == D3DFMT_DXT2 ||
        actualFormat == D3DFMT_DXT3 ||
        actualFormat == D3DFMT_DXT4 ||
        actualFormat == D3DFMT_DXT5)
    {
        flags |= CKRST_TEXTURE_ALPHA;
    }

    // Add dynamic flag if applicable
    if (param->IsDynamic)
    {
        flags |= CKRST_TEXTURE_HINTPROCEDURAL;
    }
    else
    {
        flags |= CKRST_TEXTURE_HINTSTATIC;
    }

    // Add managed flag if applicable
    if (pool == D3DPOOL_MANAGED)
    {
        flags |= CKRST_TEXTURE_MANAGED;
    }

    desc->Flags = flags;
    desc->Format.Width = surfaceDesc.Width;
    desc->Format.Height = surfaceDesc.Height;

    D3DLOCKED_RECT lockedRect;
    if (SUCCEEDED(pTexture->LockRect(0, &lockedRect, NULL, D3DLOCK_READONLY)))
    {
        desc->Format.BytesPerLine = lockedRect.Pitch;
        pTexture->UnlockRect(0);
    }
    else
    {
        // Fallback to an estimate if locking fails
        int bytesPerPixel = desc->Format.BitsPerPixel / 8;
        desc->Format.BytesPerLine = ((surfaceDesc.Width * bytesPerPixel) + 3) & ~3;
    }

    desc->MipMapCount = pTexture->GetLevelCount() - 1;
    m_Textures[Texture] = desc;

    return TRUE;
}

void CKDX9RasterizerContext::RestoreWindowStyle(HWND Window, LONG originalStyle, CKBOOL wasFullscreen)
{
    if (Window && wasFullscreen)
    {
        SetWindowLongA(Window, GWL_STYLE, originalStyle);
    }
}

CKBOOL CKDX9RasterizerContext::UpdateDeviceProperties()
{
    if (!m_Device)
        return FALSE;

    // Get back buffer information
    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        VxImageDescEx desc;
        D3DSURFACE_DESC surfaceDesc;
        if (SUCCEEDED(pBackBuffer->GetDesc(&surfaceDesc)))
        {
            SAFERELEASE(pBackBuffer);
            m_PixelFormat = D3DFormatToVxPixelFormat(surfaceDesc.Format);
            VxPixelFormat2ImageDesc(m_PixelFormat, desc);
            m_Bpp = desc.BitsPerPixel;
            m_Width = surfaceDesc.Width;
            m_Height = surfaceDesc.Height;
        }
        else
        {
            SAFERELEASE(pBackBuffer);
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }

    // Get depth/stencil information
    IDirect3DSurface9 *pStencilSurface = NULL;
    if (SUCCEEDED(m_Device->GetDepthStencilSurface(&pStencilSurface)))
    {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(pStencilSurface->GetDesc(&desc)))
        {
            SAFERELEASE(pStencilSurface);
            m_ZBpp = DepthBitPerPixelFromFormat(desc.Format, &m_StencilBpp);
        }
        else
        {
            SAFERELEASE(pStencilSurface);
            return FALSE;
        }
    }
    else
    {
        // Not fatal - some renderers might work without depth buffer
        m_ZBpp = 0;
        m_StencilBpp = 0;
    }

    return TRUE;
}

CKBOOL CKDX9RasterizerContext::InitializeDeviceStates()
{
    if (!m_Device)
        return FALSE;

    if (!SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, TRUE) ||
        !SetRenderState(VXRENDERSTATE_LOCALVIEWER, TRUE) ||
        !SetRenderState(VXRENDERSTATE_COLORVERTEX, FALSE))
    {
        return FALSE;
    }

    return TRUE;
}

void CKDX9RasterizerContext::DestroyDevice()
{
    if (m_Owner->m_FullscreenContext == this)
        m_Owner->m_FullscreenContext = NULL;

    SAFERELEASE(m_Device);
}

HRESULT CKDX9RasterizerContext::ResetDevice()
{
    // Prepare for reset by releasing resources
    FlushNonManagedObjects();

    HRESULT hr = m_Device->Reset(&m_PresentParams);
    if (FAILED(hr))
        return hr;

    // Update device properties
    UpdateDeviceProperties();

    // Restore device states
    InitializeDeviceStates();

    return hr;
}

void CKDX9RasterizerContext::ConfigureMultisampling()
{
    // Disable multisampling if not requested
    if (m_Antialias == 0)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        m_PresentParams.MultiSampleQuality = 0;
        return;
    }

    // Determine requested sampling level
    D3DMULTISAMPLE_TYPE requestedType = (m_Antialias < D3DMULTISAMPLE_2_SAMPLES || 
                                         m_Antialias > D3DMULTISAMPLE_16_SAMPLES) ? 
        D3DMULTISAMPLE_2_SAMPLES : (D3DMULTISAMPLE_TYPE)m_Antialias;

    // Try each level from requested down to 2x
    for (int type = requestedType; type >= D3DMULTISAMPLE_2_SAMPLES; --type)
    {
        DWORD qualityLevels = 0;
        if (SUCCEEDED(m_Owner->m_D3D9->CheckDeviceMultiSampleType(
            static_cast<CKDX9RasterizerDriver *>(m_Driver)->m_AdapterIndex, 
            D3DDEVTYPE_HAL,
            m_PresentParams.BackBufferFormat, 
            m_PresentParams.Windowed, 
            (D3DMULTISAMPLE_TYPE)type,
            &qualityLevels)))
        {
            m_PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)type;
            m_PresentParams.MultiSampleQuality = (qualityLevels > 0) ? (qualityLevels - 1) : 0;
            return;
        }
    }

    // No supported multisampling level found
    m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
    m_PresentParams.MultiSampleQuality = 0;
}

void CKDX9RasterizerContext::UpdateDirectXData()
{
    // Reset the structure to defaults
    m_DirectXData.DDBackBuffer = NULL;
    m_DirectXData.DDPrimaryBuffer = NULL;
    m_DirectXData.DDZBuffer = NULL;
    m_DirectXData.D3DViewport = NULL;
    m_DirectXData.DirectDraw = NULL;
    m_DirectXData.DDClipper = NULL;

    // Set device pointer
    m_DirectXData.D3DDevice = m_Device;
    m_DirectXData.DxVersion = DIRECT3D_VERSION;

    // Get back buffer
    IDirect3DSurface9 *pBackBuffer = NULL;
    HRESULT hr = m_Device ? m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer) : E_FAIL;
    if (SUCCEEDED(hr) && pBackBuffer)
    {
        SAFERELEASE(pBackBuffer);
    }

    // Get depth/stencil buffer
    IDirect3DSurface9 *pZStencilSurface = NULL;
    hr = m_Device ? m_Device->GetDepthStencilSurface(&pZStencilSurface) : E_FAIL;
    if (SUCCEEDED(hr) && pZStencilSurface)
    {
        SAFERELEASE(pZStencilSurface);
    }
}

CKBOOL CKDX9RasterizerContext::InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc *VB, CKDWORD StartIndex, CKDWORD VertexCount, WORD *indices, int indexcount, CKBOOL Clip)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (!VB || !VB->DxBuffer || !m_Device)
        return FALSE;

    if (VertexCount == 0 || (indices && indexcount == 0))
        return TRUE; // Nothing to draw, but not an error

    HRESULT hr;
    int ibstart = 0;

    // Handle indexed drawing
    if (indices)
    {
        // Ensure we have a valid index buffer of sufficient size
        CKDX9IndexBufferDesc *desc = m_IndexBuffer[Clip ? 1 : 0];
        CKBOOL needNewBuffer = FALSE;

        // Check if we need to create or recreate the index buffer
        if (!desc)
        {
            needNewBuffer = TRUE;
        }
        else if (indexcount > desc->m_MaxIndexCount)
        {
            delete desc;
            desc = NULL;
            needNewBuffer = TRUE;
        }
        else if (!desc->DxBuffer)
        {
            delete desc;
            desc = NULL;
            needNewBuffer = TRUE;
        }

        // Create a new index buffer if needed
        if (needNewBuffer)
        {
            desc = new CKDX9IndexBufferDesc;
            if (!desc)
                return FALSE;

            // Calculate appropriate buffer size (not too small, not too large)
            int maxIndexCount = indexcount + 100;
            if (maxIndexCount <= 10000)
                maxIndexCount = 10000;

            // Set up usage flags
            DWORD usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
            if (m_SoftwareVertexProcessing)
                usage |= D3DUSAGE_SOFTWAREPROCESSING;

            // Create the DirectX index buffer
            hr = m_Device->CreateIndexBuffer(
                2 * maxIndexCount,    // SIZE IN BYTES (2 bytes per WORD index)
                usage,                // Usage flags
                D3DFMT_INDEX16,       // Format (16-bit indices)
                D3DPOOL_DEFAULT,      // Memory pool
                &desc->DxBuffer,      // Resulting buffer
                NULL                  // Shared handle (not used)
            );
            if (FAILED(hr) || !desc->DxBuffer)
            {
                delete desc;
                return FALSE;
            }

            desc->m_MaxIndexCount = maxIndexCount;
            desc->m_CurrentICount = 0;
            m_IndexBuffer[Clip ? 1 : 0] = desc;
        }

        // Lock the index buffer for writing
        void *pbData = NULL;
        CKBOOL lockSuccess = FALSE;

        // Try to append data if there's room
        if (indexcount + desc->m_CurrentICount <= desc->m_MaxIndexCount)
        {
            hr = desc->DxBuffer->Lock(
                2 * desc->m_CurrentICount,   // Offset in bytes
                2 * indexcount,              // Size to lock in bytes
                &pbData,                     // Pointer to receive data
                D3DLOCK_NOOVERWRITE          // Don't overwrite existing data
            );
            if (SUCCEEDED(hr) && pbData)
            {
                ibstart = desc->m_CurrentICount;
                desc->m_CurrentICount += indexcount;
                lockSuccess = TRUE;
            }
        }

        // If appending failed, try discarding and starting fresh
        if (!lockSuccess)
        {
            hr = desc->DxBuffer->Lock(
                0,                  // Start from beginning
                2 * indexcount,     // Size to lock in bytes
                &pbData,            // Pointer to receive data
                D3DLOCK_DISCARD     // Discard previous contents
            );
            if (SUCCEEDED(hr) && pbData)
            {
                ibstart = 0;
                desc->m_CurrentICount = indexcount;
                lockSuccess = TRUE;
            }
        }

        // Copy index data and unlock
        if (lockSuccess && pbData)
        {
            memcpy(pbData, indices, 2 * indexcount);
            hr = desc->DxBuffer->Unlock();
            if (FAILED(hr))
                return FALSE;
        }
        else
        {
            // If we couldn't lock the buffer, we can't draw
            return FALSE;
        }
    }

    // Set up vertex streams (positions, normals, colors, etc.)
    SetupStreams(VB->DxBuffer, VB->m_VertexFormat, 0, VB->m_VertexSize);

    // Calculate primitive count based on primitive type
    int primitiveCount = indices ? indexcount : VertexCount;

    // Adjust primitive count based on primitive type
    switch (pType)
    {
        case VX_LINELIST:
            primitiveCount /= 2;
            break;
        case VX_LINESTRIP:
            primitiveCount = max(0, primitiveCount - 1);
            break;
        case VX_TRIANGLELIST:
            primitiveCount /= 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            primitiveCount = max(0, primitiveCount - 2);
            break;
        // Point lists don't need adjustment
        case VX_POINTLIST:
        default:
            break;
    }

    // Ensure we have at least one primitive to draw
    if (primitiveCount <= 0)
        return TRUE; // Nothing to draw, but not an error

    // Draw non-indexed primitives
    if (!indices || pType == VX_POINTLIST)
    {
        hr = m_Device->DrawPrimitive(
            (D3DPRIMITIVETYPE)pType,  // Primitive type
            StartIndex,               // Starting vertex
            primitiveCount            // Number of primitives
        );
        return SUCCEEDED(hr);
    }

    // Draw indexed primitives
    hr = m_Device->SetIndices(m_IndexBuffer[Clip ? 1 : 0]->DxBuffer);
    if (FAILED(hr))
        return FALSE;

    hr = m_Device->DrawIndexedPrimitive(
        (D3DPRIMITIVETYPE)pType,  // Primitive type
        StartIndex,               // Base vertex index
        0,                        // Minimum vertex index
        VertexCount,              // Number of vertices
        ibstart,                  // Start index
        primitiveCount            // Number of primitives
    );
    return SUCCEEDED(hr);
}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VOffset, CKDWORD VSize)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif

    if (!m_Device || !Buffer || VSize == 0)
        return;

    HRESULT hr = S_OK;
    CKBOOL useFixedPipeline = TRUE;

    // Check if we're using programmable vertex pipeline
    if (m_CurrentVertexShaderCache != 0 && m_CurrentVertexShaderCache < m_VertexShaders.Size())
    {
        CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(m_VertexShaders[m_CurrentVertexShaderCache]);

        // Verify shader exists and is valid
        if (desc && desc->DxShader)
        {
            // Get or create appropriate vertex declaration for this shader and format
            IDirect3DVertexDeclaration9 *pDecl = NULL;

            // Look for existing declaration in cache
            XNHashTable<LPDIRECT3DVERTEXDECLARATION9, DWORD>::Iterator it = m_VertexDeclarations.Find(VFormat);
            if (it != m_VertexDeclarations.End())
            {
                // Use cached declaration if found
                pDecl = *it;
            }
            else
            {
                // Create new declaration if not found
                if (CreateVertexDeclaration(VFormat, &pDecl) && pDecl)
                {
                    // Store for future use
                    m_VertexDeclarations.Insert(VFormat, pDecl);
                }
            }

            // Set up programmable pipeline if we have a valid declaration
            if (pDecl)
            {
                // First, set vertex declaration (vertex format for shader)
                hr = m_Device->SetVertexDeclaration(pDecl);
                if (SUCCEEDED(hr))
                {
                    // Then set vertex shader
                    hr = m_Device->SetVertexShader(desc->DxShader);
                    if (SUCCEEDED(hr))
                    {
                        // Successfully set up programmable pipeline
                        useFixedPipeline = FALSE;
                    }
                }
            }
        }
    }

    // Fall back to fixed function pipeline if needed
    if (useFixedPipeline)
    {
        // Reset to fixed function pipeline if previously using programmable pipeline
        if (m_CurrentVertexShaderCache != 0)
        {
            m_CurrentVertexShaderCache = 0;
            hr = m_Device->SetVertexShader(NULL);
            if (FAILED(hr))
                return;
        }

        // Update FVF if needed
        if (VFormat != m_CurrentVertexFormatCache)
        {
            m_CurrentVertexFormatCache = VFormat;
            hr = m_Device->SetFVF(VFormat);
            if (FAILED(hr))
                return;
        }
    }

    // Set vertex buffer if it's changed
    if (Buffer != m_CurrentVertexBufferCache || m_CurrentVertexSizeCache != VSize)
    {
        // Set the vertex buffer as the data source for stream 0
        hr = m_Device->SetStreamSource(0, Buffer, VOffset, VSize);
        if (SUCCEEDED(hr))
        {
            // Update cache values
            m_CurrentVertexBufferCache = Buffer;
            m_CurrentVertexSizeCache = VSize;
        }
        else
        {
            // Clear cache on failure
            m_CurrentVertexBufferCache = NULL;
            m_CurrentVertexSizeCache = 0;
        }
    }
}

CKBOOL CKDX9RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!DesiredFormat || !m_Device)
        return FALSE;

    if (Texture >= m_Textures.Size())
        return FALSE;

    // Adjust mipmap count (1 means no mipmaps)
    if (DesiredFormat->MipMapCount == 1)
        DesiredFormat->MipMapCount = 0;

    // Get driver and determine appropriate texture format
    CKDX9RasterizerDriver *driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
    if (!driver)
        return FALSE;

    CKDWORD flags = DesiredFormat->Flags;
    CKBOOL isRenderTarget = (flags & CKRST_TEXTURE_RENDERTARGET) != 0;
    CKBOOL isCubeMap = (flags & CKRST_TEXTURE_CUBEMAP) != 0;

    // Determine usage flags
    DWORD usage = 0;
    if (flags & CKRST_TEXTURE_HINTPROCEDURAL)
        usage |= D3DUSAGE_DYNAMIC;
    if (isRenderTarget)
        usage |= D3DUSAGE_RENDERTARGET;

    // Find appropriate format for the texture
    D3DFORMAT format = driver->FindNearestTextureFormat(DesiredFormat, m_PresentParams.BackBufferFormat, usage);
    if (format == D3DFMT_UNKNOWN)
    {
        if (flags & CKRST_TEXTURE_RENDERTARGET)
            format = m_PresentParams.BackBufferFormat;
        else if (flags & CKRST_TEXTURE_ALPHA)
            format = D3DFMT_A8R8G8B8;
        else
            format = D3DFMT_X8R8G8B8;
    }

    // Calculate dimensions based on hardware capabilities
    int width = DesiredFormat->Format.Width;
    int height = DesiredFormat->Format.Height;
    CKDWORD textureCaps = driver->m_3DCaps.TextureCaps;

    // Power of 2 adjustment if required
    if ((textureCaps & CKRST_TEXTURECAPS_POW2) != 0)
    {
        // Find next power of 2 for dimensions
        width = 1 << GetMsb(width, 15);
        height = 1 << GetMsb(height, 15);
    }

    // Square texture adjustment if required
    if ((isCubeMap || (textureCaps & CKRST_TEXTURECAPS_SQUAREONLY) != 0) && width != height)
    {
        // Make dimensions square by taking the larger value
        width = height = max(width, height);
    }

    // Clamp dimensions to hardware limits
    width = max(width, driver->m_3DCaps.MinTextureWidth);
    width = min(width, driver->m_3DCaps.MaxTextureWidth);
    height = max(height, driver->m_3DCaps.MinTextureHeight);
    height = min(height, driver->m_3DCaps.MaxTextureHeight);

    // Determine mipmap levels and memory pool
    UINT levels = DesiredFormat->MipMapCount != 0 ? DesiredFormat->MipMapCount + 1 : 1;

    // Render targets must be in default pool
    if (isRenderTarget)
        flags &= ~CKRST_TEXTURE_MANAGED;

    // Choose appropriate memory pool
    D3DPOOL pool;
    if (flags & CKRST_TEXTURE_MANAGED)
        pool = D3DPOOL_MANAGED; // Managed textures
    else
        pool = D3DPOOL_DEFAULT; // Default for other textures

    // Release existing texture if present to prevent memory leaks
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (desc)
    {
        delete desc;
        desc = NULL;
        m_Textures[Texture] = NULL;
    }

    // Create new texture descriptor
    desc = new CKDX9TextureDesc();
    if (!desc)
        return FALSE;

    LPDIRECT3DSURFACE9 originalRenderTarget = NULL;
    LPDIRECT3DSURFACE9 originalDepthStencil = NULL;

    if (isRenderTarget)
    {
        m_Device->GetRenderTarget(0, &originalRenderTarget);
        m_Device->GetDepthStencilSurface(&originalDepthStencil);
    }

    HRESULT hr = S_OK;
    CKBOOL success = FALSE;

    // Create regular texture or cube texture based on flags
    if (!isCubeMap)
    {
        // Create regular 2D texture
        LPDIRECT3DTEXTURE9 pTexture = NULL;
        hr = m_Device->CreateTexture(width, height, levels, usage, format, pool, &pTexture, NULL);
        if (SUCCEEDED(hr) && pTexture)
        {
            // Get texture properties
            D3DSURFACE_DESC surfaceDesc = {};
            hr = pTexture->GetLevelDesc(0, &surfaceDesc);
            if (SUCCEEDED(hr))
            {
                // Set texture descriptor properties
                desc->DxTexture = pTexture;
                D3DFormatToTextureDesc(surfaceDesc.Format, desc);
                desc->Flags = flags;
                desc->Format.Width = surfaceDesc.Width;
                desc->Format.Height = surfaceDesc.Height;

                D3DLOCKED_RECT lockedRect;
                if (SUCCEEDED(desc->DxTexture->LockRect(0, &lockedRect, NULL, D3DLOCK_READONLY)))
                {
                    // Set BytesPerLine to the actual pitch from the driver
                    desc->Format.BytesPerLine = lockedRect.Pitch;
                    desc->DxTexture->UnlockRect(0);
                }
                else
                {
                    // Fallback to an aligned calculation if locking fails
                    int bytesPerPixel = desc->Format.BitsPerPixel / 8;
                    desc->Format.BytesPerLine = ((width * bytesPerPixel) + 3) & ~3; // 4-byte alignment
                }

                desc->MipMapCount = pTexture->GetLevelCount() - 1;
                m_Textures[Texture] = desc;
                success = TRUE;
            }
            else
            {
                // Failed to get level description
                SAFERELEASE(pTexture);
            }
        }
    }
    else
    {
        // Create cube texture
        LPDIRECT3DCUBETEXTURE9 pCubeTexture = NULL;
        hr = m_Device->CreateCubeTexture(width, levels, usage, format, pool, &pCubeTexture, NULL);
        if (SUCCEEDED(hr) && pCubeTexture)
        {
            // Get texture properties
            D3DSURFACE_DESC surfaceDesc = {};
            hr = pCubeTexture->GetLevelDesc(0, &surfaceDesc);
            if (SUCCEEDED(hr))
            {
                // Set texture descriptor properties
                desc->DxCubeTexture = pCubeTexture;
                D3DFormatToTextureDesc(surfaceDesc.Format, desc);
                desc->Flags = flags | CKRST_TEXTURE_CUBEMAP; // Ensure flag is set
                desc->Format.Width = surfaceDesc.Width;
                desc->Format.Height = surfaceDesc.Height;

                D3DLOCKED_RECT lockedRect;
                if (SUCCEEDED(desc->DxTexture->LockRect(0, &lockedRect, NULL, D3DLOCK_READONLY)))
                {
                    // Set BytesPerLine to the actual pitch from the driver
                    desc->Format.BytesPerLine = lockedRect.Pitch;
                    desc->DxTexture->UnlockRect(0);
                }
                else
                {
                    // Fallback to an aligned calculation if locking fails
                    int bytesPerPixel = desc->Format.BitsPerPixel / 8;
                    desc->Format.BytesPerLine = ((width * bytesPerPixel) + 3) & ~3; // 4-byte alignment
                }

                desc->MipMapCount = pCubeTexture->GetLevelCount() - 1;
                m_Textures[Texture] = desc;
                success = TRUE;
            }
            else
            {
                // Failed to get level description
                SAFERELEASE(pCubeTexture);
            }
        }
    }

    // Restore original render target if we're creating a render target
    if (isRenderTarget && success)
    {
        // For render targets, set up the appropriate texture pointers
        if (desc->DxTexture && !isCubeMap)
        {
            // Use the same texture for rendering to avoid resource conflicts
            desc->DxRenderTexture = desc->DxTexture;
        }
        
        // Restore the original render target
        if (originalRenderTarget)
        {
            m_Device->SetRenderTarget(0, originalRenderTarget);
            SAFERELEASE(originalRenderTarget);
        }
        
        if (originalDepthStencil)
        {
            m_Device->SetDepthStencilSurface(originalDepthStencil);
            SAFERELEASE(originalDepthStencil);
        }
    }
    else
    {
        // Clean up render target resources even if we're not creating a render target
        SAFERELEASE(originalRenderTarget);
        SAFERELEASE(originalDepthStencil);
    }

    // Ensure device is in a clean state
    if (success)
    {
        // Reset any texture bindings potentially affected by this creation
        for (int i = 0; i < m_Driver->m_3DCaps.MaxNumberTextureStage; ++i)
        {
            // Only unbind stages that might have the new texture (to avoid unnecessary state changes)
            LPDIRECT3DBASETEXTURE9 boundTexture = NULL;
            if (SUCCEEDED(m_Device->GetTexture(i, &boundTexture)))
            {
                if (boundTexture)
                {
                    SAFERELEASE(boundTexture);
                    m_Device->SetTexture(i, NULL);
                }
            }
        }
        
        // If this is the first texture created, or after device reset, also verify scene state
        if (Texture == 1 || m_InCreateDestroy)
        {
            // End scene if it was active to ensure consistent state
            if (m_SceneBegined)
            {
                m_Device->EndScene();
                m_SceneBegined = FALSE;
            }
        }
    }

    // Clean up on failure
    if (!success)
        delete desc;
    return success;
}

CKBOOL CKDX9RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
#if LOGGING && LOG_CREATEVERTEXSHADER
    fprintf(stderr, "create vertex shader %d\n", VShader);
#endif
    if (VShader >= m_VertexShaders.Size() || DesiredFormat == 0)
        return FALSE;

    if (!m_Device)
        return FALSE;

    // Check if the function data is valid
    if (!DesiredFormat->m_Function || DesiredFormat->m_FunctionSize == 0)
        return FALSE;

    // Case 1: DesiredFormat is already the shader in our array (update existing)
    if (DesiredFormat == m_VertexShaders[VShader])
    {
        CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat);
    }

    // Case 2: Need to create a new shader
    CKDX9VertexShaderDesc *newDesc = new CKDX9VertexShaderDesc;
    if (!newDesc)
        return FALSE;

    // Set initial data
    newDesc->Owner = this;
    newDesc->m_Function = NULL;
    newDesc->m_FunctionSize = 0;

    // Create a copy of the shader function data to avoid ownership issues
    if (DesiredFormat->m_FunctionSize > 0 && DesiredFormat->m_Function)
    {
        CKDWORD dwordCount = (DesiredFormat->m_FunctionSize + sizeof(CKDWORD) - 1) / sizeof(CKDWORD);
        newDesc->m_Function = new CKDWORD[dwordCount];
        if (!newDesc->m_Function)
        {
            delete newDesc;
            return FALSE;
        }
        memcpy(newDesc->m_Function, DesiredFormat->m_Function, DesiredFormat->m_FunctionSize);
        newDesc->m_FunctionSize = DesiredFormat->m_FunctionSize;
    }
    else
    {
        delete newDesc;
        return FALSE;
    }

    // Try to create the actual shader
    HRESULT hr = m_Device->CreateVertexShader(newDesc->m_Function, &newDesc->DxShader);
    if (FAILED(hr))
    {
        // Handle device lost case with a retry
        if (hr == D3DERR_DEVICELOST)
        {
            // Keep the descriptor but mark it for recreation when device is recovered
            newDesc->DxShader = NULL;
        }
        else
        {
            // Clean up on other errors
            delete newDesc;
            return FALSE;
        }
    }

    // Clean up any existing shader
    CKVertexShaderDesc *oldShader = m_VertexShaders[VShader];
    if (oldShader)
    {
        delete oldShader;
    }

    // Store the new shader
    m_VertexShaders[VShader] = newDesc;
    return (newDesc->DxShader != NULL);
}

CKBOOL CKDX9RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
#if LOGGING && LOG_CREATEPIXELSHADER
    fprintf(stderr, "create pixel shader %d\n", PShader);
#endif
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return FALSE;

    // Validate required resources
    if (!m_Device)
        return FALSE;

    // Check if the function data is valid
    if (!DesiredFormat->m_Function || DesiredFormat->m_FunctionSize == 0)
        return FALSE;

    // Case 1: DesiredFormat is already the shader in our array (update existing)
    if (DesiredFormat == m_PixelShaders[PShader])
    {
        CKDX9PixelShaderDesc *desc = static_cast<CKDX9PixelShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat);
    }

    // Case 2: Need to create a new shader
    CKDX9PixelShaderDesc *newDesc = new CKDX9PixelShaderDesc;
    if (!newDesc)
        return FALSE;

    // Set initial data
    newDesc->Owner = this;
    newDesc->m_Function = NULL;
    newDesc->m_FunctionSize = 0;

    // Create a copy of the shader function data to avoid ownership issues
    if (DesiredFormat->m_FunctionSize > 0 && DesiredFormat->m_Function)
    {
        CKDWORD dwordCount = (DesiredFormat->m_FunctionSize + sizeof(CKDWORD) - 1) / sizeof(CKDWORD);
        newDesc->m_Function = new CKDWORD[dwordCount];
        if (!newDesc->m_Function)
        {
            delete newDesc;
            return FALSE;
        }
        memcpy(newDesc->m_Function, DesiredFormat->m_Function, DesiredFormat->m_FunctionSize);
        newDesc->m_FunctionSize = DesiredFormat->m_FunctionSize;
    }
    else
    {
        delete newDesc;
        return FALSE;
    }

    // Try to create the actual shader
    HRESULT hr = m_Device->CreatePixelShader(newDesc->m_Function, &newDesc->DxShader);
    if (FAILED(hr))
    {
        // Handle device lost case with a retry
        if (hr == D3DERR_DEVICELOST)
        {
            // Keep the descriptor but mark it for recreation when device is recovered
            newDesc->DxShader = NULL;
        }
        else
        {
            // Clean up on other errors
            delete newDesc;
            return FALSE;
        }
    }

    // Clean up any existing shader
    CKPixelShaderDesc *oldShader = m_PixelShaders[PShader];
    if (oldShader)
    {
        delete oldShader;
    }

    // Store the new shader
    m_PixelShaders[PShader] = newDesc;
    return (newDesc->DxShader != NULL);
}

CKBOOL CKDX9RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;

    DWORD usage = D3DUSAGE_WRITEONLY;
    if ((DesiredFormat->m_Flags & CKRST_VB_DYNAMIC) != 0)
        usage |= D3DUSAGE_DYNAMIC;

    // Add software processing flag if necessary (for consistency with vertex buffers)
    if (m_SoftwareVertexProcessing)
        usage |= D3DUSAGE_SOFTWAREPROCESSING;

    // Check if we're updating an existing buffer
    CKDX9IndexBufferDesc *existingDesc = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (existingDesc == DesiredFormat)
    {
        // We're updating the same buffer descriptor
        LPDIRECT3DINDEXBUFFER9 oldBuffer = existingDesc->DxBuffer;

        LPDIRECT3DINDEXBUFFER9 newBuffer = NULL;
        if (FAILED(m_Device->CreateIndexBuffer(2 * DesiredFormat->m_MaxIndexCount, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &newBuffer, NULL)))
        {
            return FALSE;
        }

        // Update descriptor
        existingDesc->DxBuffer = newBuffer;
        existingDesc->m_Flags |= CKRST_VB_VALID;

        SAFERELEASE(oldBuffer);
        return TRUE;
    }

    LPDIRECT3DINDEXBUFFER9 buffer = NULL;
    if (FAILED(m_Device->CreateIndexBuffer(2 * DesiredFormat->m_MaxIndexCount, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &buffer, NULL)))
    {
        return FALSE;
    }

    // Clean up existing buffer if present
    if (m_IndexBuffers[IB])
        delete m_IndexBuffers[IB];

    CKDX9IndexBufferDesc *desc = new CKDX9IndexBufferDesc;
    if (!desc)
    {
        SAFERELEASE(buffer);
        return FALSE;
    }

    desc->m_CurrentICount = DesiredFormat->m_CurrentICount;
    desc->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount;
    desc->m_Flags = DesiredFormat->m_Flags | CKRST_VB_VALID;
    desc->DxBuffer = buffer;
    m_IndexBuffers[IB] = desc;
    return TRUE;
}

CKBOOL CKDX9RasterizerContext::CreateVertexDeclaration(CKDWORD VFormat, LPDIRECT3DVERTEXDECLARATION9 *ppDecl)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!ppDecl || !m_Device)
        return FALSE;

    *ppDecl = NULL;

    // Try to use D3DX utility function first (preferred method)
    if (D3DXDeclaratorFromFVF)
    {
        D3DVERTEXELEMENT9 declarator[MAX_FVF_DECL_SIZE];
        memset(declarator, 0, sizeof(declarator));

        HRESULT hr = D3DXDeclaratorFromFVF(VFormat, declarator);
        if (SUCCEEDED(hr))
        {
            hr = m_Device->CreateVertexDeclaration(declarator, ppDecl);
            if (SUCCEEDED(hr))
                return TRUE;
        }
    }

    // Manual declaration creation as fallback

    // Limit declaration element count (D3D9 supports up to 16 elements plus END)
    const int MAX_DECLARATION_ELEMENTS = 17;
    D3DVERTEXELEMENT9 declaration[MAX_FVF_DECL_SIZE];
    memset(declaration, 0, sizeof(declaration));

    // Track current element index and byte offset
    int elementIndex = 0;
    WORD offset = 0;
    BYTE stream = 0; // Default stream index

    // -------------------- POSITION ELEMENTS --------------------
    // Handle various position formats
    if (VFormat & CKRST_VF_POSITION)
    {
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_FLOAT3;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_POSITION;
        declaration[elementIndex].UsageIndex = 0;

        offset += 12; // 3 floats
        elementIndex++;
    }
    else if (VFormat & CKRST_VF_RASTERPOS)
    {
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_FLOAT4;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_POSITION;
        declaration[elementIndex].UsageIndex = 0;

        offset += 16; // 4 floats
        elementIndex++;
    }
    else if (VFormat & CKRST_VF_POSITIONMASK)
    {
        // Handle vertex blending with multiple weights
        int weightCount = CKRST_VF_GETWCOUNT(VFormat);
        if (weightCount > 0)
        {
            // Position (always 3 floats)
            declaration[elementIndex].Stream = stream;
            declaration[elementIndex].Offset = offset;
            declaration[elementIndex].Type = D3DDECLTYPE_FLOAT3;
            declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
            declaration[elementIndex].Usage = D3DDECLUSAGE_POSITION;
            declaration[elementIndex].UsageIndex = 0;
            offset += 12;
            elementIndex++;

            // Add blend weights (1-4 weights supported)
            declaration[elementIndex].Stream = stream;
            declaration[elementIndex].Offset = offset;

            // Map number of weights to appropriate declaration type
            switch (weightCount)
            {
                case 1: declaration[elementIndex].Type = D3DDECLTYPE_FLOAT1; offset += 4; break;
                case 2: declaration[elementIndex].Type = D3DDECLTYPE_FLOAT2; offset += 8; break;
                case 3: declaration[elementIndex].Type = D3DDECLTYPE_FLOAT3; offset += 12; break;
                case 4: default: declaration[elementIndex].Type = D3DDECLTYPE_FLOAT4; offset += 16; break;
            }

            declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
            declaration[elementIndex].Usage = D3DDECLUSAGE_BLENDWEIGHT;
            declaration[elementIndex].UsageIndex = 0;
            elementIndex++;

            // Add blend indices if matrix palette is used
            if (VFormat & CKRST_VF_MATRIXPAL)
            {
                declaration[elementIndex].Stream = stream;
                declaration[elementIndex].Offset = offset;
                declaration[elementIndex].Type = D3DDECLTYPE_UBYTE4;
                declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
                declaration[elementIndex].Usage = D3DDECLUSAGE_BLENDINDICES;
                declaration[elementIndex].UsageIndex = 0;
                offset += 4;
                elementIndex++;
            }
        }
    }

    // -------------------- NORMAL ELEMENTS --------------------
    // Handle normal data (tangent/binormal support for bump mapping)
    if (VFormat & CKRST_VF_NORMAL)
    {
        // Primary normal vector
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_FLOAT3;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_NORMAL;
        declaration[elementIndex].UsageIndex = 0;
        offset += 12;
        elementIndex++;

        // Check for tangent/binormal vectors (could be indicated by special combination)
        // Tangent and binormal data handling could be added here if necessary
        // This would require defining custom flags or encoding in upper bits of VFormat
    }

    // -------------------- POINT SIZE --------------------
    if (VFormat & CKRST_VF_PSIZE)
    {
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_FLOAT1;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_PSIZE;
        declaration[elementIndex].UsageIndex = 0;
        offset += 4;
        elementIndex++;
    }

    // -------------------- COLOR ELEMENTS --------------------
    if (VFormat & CKRST_VF_DIFFUSE)
    {
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_D3DCOLOR;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_COLOR;
        declaration[elementIndex].UsageIndex = 0;
        offset += 4;
        elementIndex++;
    }

    if (VFormat & CKRST_VF_SPECULAR)
    {
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = D3DDECLTYPE_D3DCOLOR;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = D3DDECLUSAGE_COLOR;
        declaration[elementIndex].UsageIndex = 1;
        offset += 4;
        elementIndex++;
    }

    // -------------------- TEXTURE COORDINATE ELEMENTS --------------------
    // Get number of texture coordinate sets in the format
    DWORD texCount = CKRST_VF_GETTEXCOUNT(VFormat);

    // Process each texture coordinate set
    for (DWORD i = 0; i < texCount && i < 8 && elementIndex < MAX_DECLARATION_ELEMENTS - 1; i++)
    {
        // Get number of components for this texture coordinate set
        DWORD texDimension = 2; // Default is 2D (UV)

        // Check for texture coordinate dimension override (stored in bits 16-31)
        DWORD texFormat = (VFormat >> (16 + i * 2)) & 0x3;
        switch (texFormat)
        {
            case CKRST_VF_TEXFORMAT1: texDimension = 1; break; // 1D (U)
            case CKRST_VF_TEXFORMAT3: texDimension = 3; break; // 3D (UVW)
            case CKRST_VF_TEXFORMAT4: texDimension = 4; break; // 4D (UVWQ)
            default: texDimension = 2; break;                  // 2D (UV)
        }

        // Map dimension to D3D declaration type
        D3DDECLTYPE declType;
        switch (texDimension)
        {
            case 1: declType = D3DDECLTYPE_FLOAT1; break;
            case 3: declType = D3DDECLTYPE_FLOAT3; break;
            case 4: declType = D3DDECLTYPE_FLOAT4; break;
            default: declType = D3DDECLTYPE_FLOAT2; break;
        }
        

        // Determine appropriate texture coordinate usage type
        // For special bump mapping, use different semantics
        D3DDECLUSAGE usage = D3DDECLUSAGE_TEXCOORD;

        // Set up the declaration element
        declaration[elementIndex].Stream = stream;
        declaration[elementIndex].Offset = offset;
        declaration[elementIndex].Type = declType;
        declaration[elementIndex].Method = D3DDECLMETHOD_DEFAULT;
        declaration[elementIndex].Usage = usage;
        declaration[elementIndex].UsageIndex = i;

        // Update offset for next element
        offset += texDimension * sizeof(float);
        elementIndex++;
    }

    // Add terminator element (required by Direct3D 9)
    declaration[elementIndex].Stream = 0xFF;
    declaration[elementIndex].Offset = 0;
    declaration[elementIndex].Type = D3DDECLTYPE_UNUSED;
    declaration[elementIndex].Method = 0;
    declaration[elementIndex].Usage = 0;
    declaration[elementIndex].UsageIndex = 0;

    // Create the vertex declaration
    HRESULT hr = m_Device->CreateVertexDeclaration(declaration, ppDecl);
    return SUCCEEDED(hr);
}

void CKDX9RasterizerContext::FlushCaches()
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Reset render state cache
    FlushRenderStateCache();
    m_InverseWinding = FALSE;

    // First release any existing state blocks to prevent memory leaks
    ReleaseStateBlocks();

#if USE_D3DSTATEBLOCKS
    // Only proceed if device is valid and in a usable state
    if (!m_Device || FAILED(m_Device->TestCooperativeLevel()))
        return;

    HRESULT hr;

    // Create new state blocks for texture filtering and blending operations
    for (int i = 0; i < 8; i++) // For each texture stage
    {
        for (int j = 0; j < 8; j++) // For each filter type
        {
            // Create min filter state blocks
            hr = m_Device->BeginStateBlock();
            if (SUCCEEDED(hr))
            {
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMinFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                SetTextureStageState(i, CKRST_TSS_MINFILTER, j);
                hr = m_Device->EndStateBlock(&m_TextureMinFilterStateBlocks[j][i]);

#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMinFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif

                // If EndStateBlock failed, ensure pointer is NULL
                if (FAILED(hr))
                    m_TextureMinFilterStateBlocks[j][i] = NULL;
            }

            // Create mag filter state blocks
            hr = m_Device->BeginStateBlock();
            if (SUCCEEDED(hr))
            {
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMagFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif
                SetTextureStageState(i, CKRST_TSS_MAGFILTER, j);
                hr = m_Device->EndStateBlock(&m_TextureMagFilterStateBlocks[j][i]);

#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMagFilterStateBlocks %d %d -> 0x%x\n", i, j, hr);
#endif

                // If EndStateBlock failed, ensure pointer is NULL
                if (FAILED(hr))
                    m_TextureMagFilterStateBlocks[j][i] = NULL;
            }
        }

        // Create texture map blend state blocks
        for (int k = 0; k < 10; k++)
        {
            hr = m_Device->BeginStateBlock();
            if (SUCCEEDED(hr))
            {
#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "begin TextureMapBlendStateBlocks %d %d -> 0x%x\n", i, k, hr);
#endif
                SetTextureStageState(i, CKRST_TSS_TEXTUREMAPBLEND, k);
                hr = m_Device->EndStateBlock(&m_TextureMapBlendStateBlocks[k][i]);

#if LOGGING && LOG_FLUSHCACHES
                fprintf(stderr, "end TextureMapBlendStateBlocks %d %d -> 0x%x\n", i, k, hr);
#endif

                // If EndStateBlock failed, ensure pointer is NULL
                if (FAILED(hr))
                    m_TextureMapBlendStateBlocks[k][i] = NULL;
            }
        }
    }
#endif
}

void CKDX9RasterizerContext::FlushNonManagedObjects()
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Skip if device is null or we're already in create/destroy process
    if (!m_Device || m_InCreateDestroy)
        return;

    HRESULT hr;

    // Clear all texture stages first
    for (int i = 0; i < m_Driver->m_3DCaps.MaxNumberTextureStage; ++i)
    {
        hr = m_Device->SetTexture(i, NULL);
        if (FAILED(hr))
        {
#if LOGGING
            fprintf(stderr, "Failed to clear texture stage %d\n", i);
#endif
        }
    }

    // Reset indices and vertex streams
    hr = m_Device->SetIndices(NULL);
    if (FAILED(hr))
    {
#if LOGGING
        fprintf(stderr, "Failed to clear index buffer\n");
#endif
    }

    // Clear vertex streams (correctly using all parameters)
    for (UINT i = 0; i < 8; ++i) // DirectX 9 typically supports 8 streams
    {
        hr = m_Device->SetStreamSource(i, NULL, 0, 0);
        if (FAILED(hr))
        {
#if LOGGING
            fprintf(stderr, "Failed to clear vertex stream %d\n", i);
#endif
        }
    }

    // Clear vertex and pixel shaders
    hr = m_Device->SetVertexShader(NULL);
    if (FAILED(hr))
    {
#if LOGGING
        fprintf(stderr, "Failed to clear vertex shader\n");
#endif
    }

    hr = m_Device->SetPixelShader(NULL);
    if (FAILED(hr))
    {
#if LOGGING
        fprintf(stderr, "Failed to clear pixel shader\n");
#endif
    }

    // Reset render target and depth buffer
    if (m_DefaultBackBuffer)
    {
        m_Device->SetRenderTarget(0, m_DefaultBackBuffer);
        if (m_DefaultDepthBuffer)
            m_Device->SetDepthStencilSurface(m_DefaultDepthBuffer);
    }

    SAFERELEASE(m_DefaultBackBuffer);
    SAFERELEASE(m_DefaultDepthBuffer);

    // Reset current texture index
    m_CurrentTextureIndex = 0;

    // Remove non-managed textures
    for (XArray<CKTextureDesc *>::Iterator it = m_Textures.Begin(); it != m_Textures.End(); ++it)
    {
        CKTextureDesc *desc = *it;
        if (desc && !(desc->Flags & CKRST_TEXTURE_MANAGED))
        {
            // For DirectX 9, ensure all texture surfaces are properly released
            CKDX9TextureDesc *dx9Desc = static_cast<CKDX9TextureDesc *>(desc);
            if (dx9Desc)
            {
                // Special handling for render target textures
                if (dx9Desc->Flags & CKRST_TEXTURE_RENDERTARGET)
                {
                    // Ensure any active render target is unbound
                    LPDIRECT3DSURFACE9 currentRT = NULL;
                    m_Device->GetRenderTarget(0, &currentRT);

                    if (currentRT)
                    {
                        // Check if this is our texture's surface
                        if (dx9Desc->DxTexture)
                        {
                            LPDIRECT3DSURFACE9 texSurface = NULL;
                            dx9Desc->DxTexture->GetSurfaceLevel(0, &texSurface);

                            if (texSurface)
                            {
                                if (texSurface == currentRT)
                                {
                                    // Reset render target to back buffer
                                    LPDIRECT3DSURFACE9 backBuffer = NULL;
                                    m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
                                    if (backBuffer)
                                    {
                                        m_Device->SetRenderTarget(0, backBuffer);
                                        SAFERELEASE(backBuffer);
                                    }
                                }
                                SAFERELEASE(texSurface);
                            }
                        }
                        SAFERELEASE(currentRT);
                    }
                }
            }

            delete desc;
            *it = NULL;
        }
    }

    // Release temporary Z-buffers
    ReleaseTempZBuffers();

    // Release vertex and index buffers, shaders
    FlushObjects(CKRST_OBJ_VERTEXBUFFER | CKRST_OBJ_INDEXBUFFER | CKRST_OBJ_VERTEXSHADER | CKRST_OBJ_PIXELSHADER);

    // Release static index buffers
    ReleaseIndexBuffers();

    // Reset vertex format cache
    ClearStreamCache();
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
    // Make sure device isn't using the index buffers
    if (m_Device)
    {
        m_Device->SetIndices(NULL);
    }

    // Properly release index buffers with proper cleanup
    for (int i = 0; i < 2; ++i)
    {
        if (m_IndexBuffer[i])
        {
            SAFERELEASE(m_IndexBuffer[i]->DxBuffer);
            delete m_IndexBuffer[i];
            m_IndexBuffer[i] = NULL;
        }
    }
}

void CKDX9RasterizerContext::ClearStreamCache()
{
    // Clear vertex stream cache
    m_CurrentVertexBufferCache = NULL;
    m_CurrentVertexSizeCache = 0;
    m_CurrentVertexFormatCache = 0;
    m_CurrentVertexShaderCache = 0;
    m_CurrentPixelShaderCache = 0;

    // Reset device stream sources if device is available
    if (m_Device)
    {
        // Clear all possible stream sources (typically up to 8 for DX9)
        for (UINT i = 0; i < 8; ++i)
        {
            m_Device->SetStreamSource(i, NULL, 0, 0);
        }

        // Reset FVF and shader states
        m_Device->SetFVF(0);
        m_Device->SetVertexShader(NULL);
    }
}

void CKDX9RasterizerContext::ReleaseScreenBackup()
{
    if (m_InCreateDestroy)
        return;

    // If in transparent mode, notify that screen backup is no longer available
    if (m_TransparentMode)
    {
        // Add dirty rect to indicate the whole screen needs redrawing
        CKRECT rect = {0, 0, (int)m_Width, (int)m_Height};
        AddDirtyRect(&rect);
    }

    SAFERELEASE(m_ScreenBackup);
}

void CKDX9RasterizerContext::ReleaseVertexDeclarations()
{
    // Make sure we're not in the middle of creating/destroying resources
    if (m_InCreateDestroy)
        return;

    // Clear any active vertex declaration from the device
    if (m_Device)
    {
        m_Device->SetVertexDeclaration(NULL);
    }

    // Release all declarations in the hash table
    for (XNHashTable<LPDIRECT3DVERTEXDECLARATION9, DWORD>::Iterator it = m_VertexDeclarations.Begin(); it != m_VertexDeclarations.End(); ++it)
    {
        LPDIRECT3DVERTEXDECLARATION9 decl = *it;
        SAFERELEASE(decl);
    }

    // Clear the hash table
    m_VertexDeclarations.Clear();

    // Reset vertex format cache to force recreation of declarations
    m_CurrentVertexFormatCache = 0;
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
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    if (!LockRect.pBits)
        return FALSE;

    // Set up target image descriptor
    VxImageDescEx desc;
    desc.Size = sizeof(VxImageDescEx);

    // Get pixel format from the DirectX surface format
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(ddsd.Format);
    if (vxpf == UNKNOWN_PF)
    {
        // Fallback to a compatible format if we couldn't determine the format
        if (ddsd.Format == D3DFMT_A8R8G8B8 || ddsd.Format == D3DFMT_X8R8G8B8)
            vxpf = _32_ARGB8888;
        else if (ddsd.Format == D3DFMT_R8G8B8)
            vxpf = _24_RGB888;
        else if (ddsd.Format == D3DFMT_R5G6B5)
            vxpf = _16_RGB565;
        else if (ddsd.Format == D3DFMT_A1R5G5B5 || ddsd.Format == D3DFMT_X1R5G5B5)
            vxpf = _16_ARGB1555;
        else
            return FALSE; // Couldn't determine a compatible format
    }

    // Fill in target descriptor
    VxPixelFormat2ImageDesc(vxpf, desc);
    desc.Width = ddsd.Width;
    desc.Height = ddsd.Height;
    desc.BytesPerLine = LockRect.Pitch;
    desc.Image = static_cast<XBYTE *>(LockRect.pBits);

    // Check if source is empty
    if (!SurfDesc.Image)
        return FALSE;

    // Check if we need format conversion
    if (vxpf != VxImageDesc2PixelFormat(SurfDesc) || desc.Width != SurfDesc.Width || desc.Height != SurfDesc.Height)
    {
        // For DXT formats, use D3DX for conversion if available
        if ((ddsd.Format >= D3DFMT_DXT1 && ddsd.Format <= D3DFMT_DXT5) && D3DXLoadSurfaceFromMemory && m_Device)
        {
            // Release current lock and use D3DX for conversion
            // (This is a hook for implementation elsewhere)
            return FALSE;
        }

        // Perform the blitting operation with format conversion
        VxDoBlit(SurfDesc, desc);
        return TRUE;
    }
    else
    {
        // Formats match - perform straight memory copy
        const int height = SurfDesc.Height;
        const int srcStride = SurfDesc.BytesPerLine;
        const int dstStride = LockRect.Pitch;

        // If stride matches, do a single copy
        if (srcStride == dstStride && srcStride == SurfDesc.Width * (SurfDesc.BitsPerPixel / 8))
        {
            memcpy(LockRect.pBits, SurfDesc.Image, srcStride * height);
        }
        else
        {
            // Copy line by line
            const int bytesPerRow = srcStride < dstStride ? srcStride : dstStride;

            XBYTE *src = SurfDesc.Image;
            XBYTE *dst = static_cast<XBYTE *>(LockRect.pBits);

            for (int y = 0; y < height; ++y)
            {
                memcpy(dst, src, bytesPerRow);
                src += srcStride;
                dst += dstStride;
            }
        }

        return TRUE;
    }
}

LPDIRECT3DSURFACE9 CKDX9RasterizerContext::GetTempZBuffer(int Width, int Height)
{
#ifdef TRACY_ENABLE
    ZoneScopedN(__FUNCTION__);
#endif
    // Validate inputs and device
    if (Width <= 0 || Height <= 0 || !m_Device)
        return NULL;

    // Find appropriate power-of-two dimensions to use as index
    int widthPow2 = 1;
    while (widthPow2 < Width)
        widthPow2 <<= 1;

    int heightPow2 = 1;
    while (heightPow2 < Height)
        heightPow2 <<= 1;

    // Calculate index in z-buffer array
    // Use most significant bits for a more evenly distributed index
    CKDWORD widthLog2 = 0;
    CKDWORD heightLog2 = 0;

    // Calculate log2 of width and height
    if (widthPow2 > 1)
        widthLog2 = GetMsb(widthPow2, 15); // Limit to 4 bits (0-15)

    if (heightPow2 > 1)
        heightLog2 = GetMsb(heightPow2, 15); // Limit to 4 bits (0-15)

    // Combine into a single index (4 bits width, 4 bits height)
    CKDWORD index = (heightLog2 << 4) | widthLog2;

    // Ensure index is within bounds
    if (index >= NBTEMPZBUFFER)
    {
        // If index too big, use the largest buffer (at index NBTEMPZBUFFER-1)
        index = NBTEMPZBUFFER - 1;
    }

    // Check if we already have a buffer at this index
    LPDIRECT3DSURFACE9 surface = m_TempZBuffers[index];
    if (surface)
    {
        // Verify surface is compatible
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(surface->GetDesc(&desc)) && desc.Width >= Width && desc.Height >= Height)
        {
            // Buffer is large enough, reuse it
            return surface;
        }

        // Not compatible - release existing buffer
        SAFERELEASE(m_TempZBuffers[index]);
    }

    // Create a new Z-buffer - use at least the requested dimensions
    HRESULT hr = m_Device->CreateDepthStencilSurface(
        widthPow2, heightPow2,
        m_PresentParams.AutoDepthStencilFormat,
        m_PresentParams.MultiSampleType, 
        m_PresentParams.MultiSampleQuality,
        TRUE, // Discard lock hint for better performance
        &m_TempZBuffers[index],
        NULL);
    if (FAILED(hr))
    {
        // Try again with no multisampling
        hr = m_Device->CreateDepthStencilSurface(
            widthPow2, heightPow2,
            m_PresentParams.AutoDepthStencilFormat,
            D3DMULTISAMPLE_NONE, 
            0,
            TRUE,
            &m_TempZBuffers[index],
            NULL);

        if (FAILED(hr))
        {
            // Final attempt with a known supported depth format
            hr = m_Device->CreateDepthStencilSurface(
                widthPow2, heightPow2,
                D3DFMT_D24S8, // Widely supported format
                D3DMULTISAMPLE_NONE,
                0,
                TRUE,
                &m_TempZBuffers[index],
                NULL);
        }
    }

    return (SUCCEEDED(hr)) ? m_TempZBuffers[index] : NULL;
}
