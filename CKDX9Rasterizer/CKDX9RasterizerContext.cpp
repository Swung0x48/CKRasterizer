#include "CKDX9Rasterizer.h"

#define LOGGING 1
#define STEP 0

#if STEP
#include <conio.h>
static bool step_mode = false;
#endif

CKDX9RasterizerContext::CKDX9RasterizerContext() :
	m_Device(nullptr),
	m_PresentParams(), m_DirectXData(),
	m_SoftwareVertexProcessing(0),
	m_ResetLastFrame(0), m_IndexBuffer{},
	m_DefaultBackBuffer(nullptr),
	m_DefaultDepthBuffer(nullptr),
	m_InCreateDestroy(1),
	m_ScreenBackup(nullptr),
	m_CurrentVertexShaderCache(0),
	m_CurrentVertexFormatCache(0),
	m_CurrentVertexBufferCache(nullptr),
	m_CurrentVertexSizeCache(0),
	m_TranslatedRenderStates{},
	m_TempZBuffers{}
{
}

CKDX9RasterizerContext::~CKDX9RasterizerContext()
{
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

BOOL CKDX9RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
	BOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
#if (STEP) || (LOGGING)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
	m_InCreateDestroy = TRUE;
    CKRECT Rect;
	if (Window)
	{
		VxGetWindowRect(Window, &Rect);
		WIN_HANDLE Parent = VxGetParent(Window);
		VxScreenToClient(Parent, reinterpret_cast<CKPOINT*>(&Rect));
		VxScreenToClient(Parent, reinterpret_cast<CKPOINT*>(&Rect.right));
	}
	if (Fullscreen)
	{
		LONG PrevStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
		SetWindowLongA((HWND)Window, GWL_STYLE, PrevStyle & ~WS_CHILDWINDOW);
	}
    if (Bpp == 16) Bpp = 32; // doesn't really matter, but just in case
    CKDX9RasterizerDriver *Driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
	ZeroMemory(&m_PresentParams, sizeof(m_PresentParams));
	m_PresentParams.hDeviceWindow = (HWND) Window;
	m_PresentParams.BackBufferWidth = Width;
	m_PresentParams.BackBufferHeight = Height;
	m_PresentParams.BackBufferCount = 1;
	m_PresentParams.Windowed = !Fullscreen;
	m_PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	m_PresentParams.EnableAutoDepthStencil = TRUE;
	m_PresentParams.FullScreen_RefreshRateInHz = Fullscreen ? RefreshRate : D3DPRESENT_RATE_DEFAULT;
	m_PresentParams.PresentationInterval = Fullscreen ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_DEFAULT;
    m_PresentParams.BackBufferFormat = Driver->FindNearestRenderTargetFormat(Bpp, !Fullscreen);
    m_PresentParams.AutoDepthStencilFormat =
        Driver->FindNearestDepthFormat(
		m_PresentParams.BackBufferFormat,
		Zbpp, StencilBpp);

	D3DDISPLAYMODEEX DisplayMode = {
        sizeof(D3DDISPLAYMODEEX),
	    (UINT)Width,
	    (UINT)Height,
	    (UINT)RefreshRate,
	    m_PresentParams.BackBufferFormat,
        D3DSCANLINEORDERING_PROGRESSIVE
	};
	DWORD BehaviorFlag = D3DCREATE_MULTITHREADED;
    if (m_Antialias == D3DMULTISAMPLE_NONE)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
    } else
    {
        for (int type = m_Antialias; type > D3DMULTISAMPLE_NONMASKABLE; type--)
        {
            if (SUCCEEDED(m_Owner->m_D3D9->CheckDeviceMultiSampleType(
                Driver->m_AdapterIndex,
				D3DDEVTYPE_HAL,
				m_PresentParams.BackBufferFormat,
				m_PresentParams.Windowed,
				m_PresentParams.MultiSampleType, NULL)))
				break;
        }
    }
    assert(m_PresentParams.MultiSampleType != D3DMULTISAMPLE_NONMASKABLE);

    if (!Driver->m_IsHTL || Driver->m_D3DCaps.VertexShaderVersion < D3DVS_VERSION(1, 0) && this->m_EnsureVertexShader)
    {
        m_SoftwareVertexProcessing = TRUE;
        BehaviorFlag |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
    else
    {
        BehaviorFlag |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        if ((Driver->m_D3DCaps.DevCaps & D3DDEVCAPS_PUREDEVICE) != 0)
            BehaviorFlag |= D3DCREATE_PUREDEVICE;
        m_SoftwareVertexProcessing = FALSE;
    }
    // screendump?
    HRESULT result =
        m_Owner->m_D3D9->CreateDeviceEx(Driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                        BehaviorFlag, &m_PresentParams, Fullscreen ? &DisplayMode : NULL, &m_Device);
    if (FAILED(result) && m_PresentParams.MultiSampleType)
    {
        m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        result = m_Owner->m_D3D9->CreateDeviceEx(Driver->m_AdapterIndex, D3DDEVTYPE_HAL, (HWND)m_Owner->m_MainWindow,
                                                 BehaviorFlag, &m_PresentParams, Fullscreen ? &DisplayMode : NULL,
                                                 &m_Device);
    }

    if (Fullscreen)
    {
        LONG PrevStyle = GetWindowLongA((HWND)Window, GWL_STYLE);
        SetWindowLongA((HWND)Window, -16, PrevStyle | WS_CHILDWINDOW);
    }
    else if (Window && !Fullscreen)
    {
        VxMoveWindow(Window, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, FALSE);
    }

	if (FAILED(result))
    {
        m_InCreateDestroy = FALSE;
        return 0;
    }
    m_Window = (HWND)Window;
    m_PosX = PosX;
    m_PosY = PosY;
	m_Fullscreen = Fullscreen;
    IDirect3DSurface9 *pBackBuffer = NULL;
    if (SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
    {
        VxImageDescEx desc;
        D3DSURFACE_DESC pDesc;
        pBackBuffer->GetDesc(&pDesc);
        pBackBuffer->Release();
        pBackBuffer = NULL;
        m_PixelFormat = D3DFormatToVxPixelFormat(pDesc.Format);
        VxPixelFormat2ImageDesc(m_PixelFormat, desc);
        m_Bpp = desc.BitsPerPixel;
        m_Width = pDesc.Width;
        m_Height = pDesc.Height;
    }
    IDirect3DSurface9 *pStencilSurface = NULL;
    if (SUCCEEDED(m_Device->GetDepthStencilSurface(&pStencilSurface)))
    {
        D3DSURFACE_DESC pDesc;
        pStencilSurface->GetDesc(&pDesc);
        pStencilSurface->Release();
        pStencilSurface = NULL;
        m_ZBpp = DepthBitPerPixelFromFormat(pDesc.Format, &m_StencilBpp);
    }
    SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, 1);
    SetRenderState(VXRENDERSTATE_LOCALVIEWER, 1);
    SetRenderState(VXRENDERSTATE_COLORVERTEX, 0);
    UpdateDirectXData();
	// this->FlushCaches();
    UpdateObjectArrays(m_Driver->m_Owner);
    ClearStreamCache();
    if (m_Fullscreen)
        m_Driver->m_Owner->m_FullscreenContext = this;
    m_InCreateDestroy = FALSE;
    return 1;
}

BOOL CKDX9RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    if (m_InCreateDestroy)
        return FALSE;
    EndScene();
    ReleaseScreenBackup();
    if ((Flags & 1) == 0)
    {
        m_PosX = PosX;
        m_PosY = PosY;
    }
    RECT Rect;
    if ((Flags & 2) == 0)
    {
        if (Width == 0 || Height == 0)
        {
            GetClientRect((HWND)m_Window, &Rect);
            Width = Rect.right - m_PosX;
            Height = Rect.bottom - m_PosY;
        }
        m_PresentParams.BackBufferWidth = Width;
        m_PresentParams.BackBufferHeight = Height;
        // 24cc5b00()
        FlushNonManagedObjects();
        ClearStreamCache();
        if (m_PresentParams.MultiSampleType == D3DMULTISAMPLE_NONE && m_Antialias != D3DMULTISAMPLE_NONE)
        {
            m_PresentParams.MultiSampleType =
                (m_Antialias < 2 || m_Antialias > 16) ? D3DMULTISAMPLE_2_SAMPLES : (D3DMULTISAMPLE_TYPE)m_Antialias;
            for (int type = m_PresentParams.MultiSampleType; type >= D3DMULTISAMPLE_2_SAMPLES; --type)
            {
                if (SUCCEEDED(m_Owner->m_D3D9->CheckDeviceMultiSampleType(
                    static_cast<CKDX9RasterizerDriver*>(m_Driver)->m_AdapterIndex, D3DDEVTYPE_HAL, m_PresentParams.BackBufferFormat,
                        m_PresentParams.Windowed, m_PresentParams.MultiSampleType, NULL)))
                    break;
                m_PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)type;
            }
        }
        if (m_PresentParams.MultiSampleType < D3DMULTISAMPLE_2_SAMPLES || m_Antialias == D3DMULTISAMPLE_NONE)
            m_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
        HRESULT hr = m_Device->Reset(&m_PresentParams);
        if (hr == D3DERR_DEVICELOST)
        {
            if (m_Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            {
                hr = m_Device->Reset(&this->m_PresentParams);
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
            hr = m_Device->Reset(&this->m_PresentParams);
        }

        UpdateDirectXData();
        // FlushCaches();
        return SUCCEEDED(hr);
    }
    return 1;
}

BOOL CKDX9RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT* rects)
{
    if (m_InCreateDestroy)
        return 0;
    BOOL result = 0;
    if (m_Device)
    {
        if (!m_TransparentMode && (Flags & CKRST_CTXCLEAR_COLOR) != 0 && m_Bpp)
            result = 1;
        if ((Flags & CKRST_CTXCLEAR_STENCIL) != 0 && m_StencilBpp)
            result |= 4;
        if ((Flags & CKRST_CTXCLEAR_DEPTH) != 0 && m_ZBpp)
            result |= 2;
        return SUCCEEDED(m_Device->Clear(RectCount, (D3DRECT*)rects, result, Ccol, Z, Stencil));
    }
    return result == 0;
}

BOOL CKDX9RasterizerContext::BackToFront(CKBOOL vsync)
{
    if (m_InCreateDestroy || !m_Device)
        return 0;
    if (m_SceneBegined)
        EndScene();
    // dword_24cff074 = vsync;
    if (vsync && !m_Fullscreen)
    {
        D3DRASTER_STATUS RasterStatus;
        HRESULT hr = m_Device->GetRasterStatus(0, &RasterStatus);
        while (SUCCEEDED(hr) && RasterStatus.InVBlank)
        {
            hr = m_Device->GetRasterStatus(0, &RasterStatus);
        }
    }

    HRESULT hr = D3DERR_INVALIDCALL;
    if (m_Driver->m_Owner->m_FullscreenContext == this)
        hr = m_Device->Present(NULL, NULL, NULL, NULL);
    else
    {
        if (!m_TransparentMode)
        {
            RECT SourceRect { 0, 0, (LONG)m_Width, (LONG)m_Height };
            RECT DestRect{(LONG)m_PosX, (LONG)m_PosY, (LONG)(m_PosX + m_Width), (LONG)(m_PosY + m_Height) };
            hr = m_Device->Present(&SourceRect, &DestRect, NULL, NULL);
        }
    }
    if (hr == D3DERR_DEVICELOST)
    {
        hr = m_Device->TestCooperativeLevel();
        if (hr == D3DERR_DEVICENOTRESET)
            Resize(m_PosX, m_PosY, m_Width, m_Height, 0);
    }
#if LOGGING
    fprintf(stderr, "buffer swap\n");
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

BOOL CKDX9RasterizerContext::BeginScene()
{
    if (m_SceneBegined)
        return 1;
    HRESULT hr = m_Device->BeginScene();
    m_SceneBegined = 1;
    return SUCCEEDED(hr);
}
//
//struct CUSTOMVERTEX
//{
//    FLOAT x, y, z;
//    FLOAT rhw;
//    DWORD color;
//    FLOAT tu, tv; // Texture coordinates
//};
//// Custom flexible vertex format (FVF) describing the custom vertex structure
//#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
BOOL CKDX9RasterizerContext::EndScene()
{
    if (!m_SceneBegined)
        return 1;
    
    /*CUSTOMVERTEX vertices[] = {
        {
            320.0f,
            50.0f,
            0.5f,
            1.0f,
            D3DCOLOR_XRGB(0, 0, 255),
        },
        {
            520.0f,
            400.0f,
            0.5f,
            1.0f,
            D3DCOLOR_XRGB(0, 255, 0),
        },
        {
            120.0f,
            400.0f,
            0.5f,
            1.0f,
            D3DCOLOR_XRGB(255, 0, 0),
        },
    };
    IDirect3DVertexBuffer9 *vb;
    assert(SUCCEEDED(
        m_Device->CreateVertexBuffer(3 * sizeof(CUSTOMVERTEX), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &vb, NULL)));
    void *pVertices = NULL;
    assert((SUCCEEDED(vb->Lock(0, 3 * sizeof(CUSTOMVERTEX), &pVertices, 0))));
    memcpy(pVertices, vertices, sizeof(CUSTOMVERTEX) * 3);
    assert((SUCCEEDED(vb->Unlock())));
    assert(SUCCEEDED(m_Device->SetStreamSource(0, vb, 0, sizeof(CUSTOMVERTEX))));
    assert(SUCCEEDED(m_Device->SetFVF(D3DFVF_CUSTOMVERTEX)));
    assert(SUCCEEDED(m_Device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1)));*/


    HRESULT hr = m_Device->EndScene();
    m_SceneBegined = 0;
    return SUCCEEDED(hr);
}

BOOL CKDX9RasterizerContext::SetLight(CKDWORD Light, CKLightData* data)
{
    // Could be a problem
    D3DLIGHT9 lightData;
    switch (data->Type)
    {
        case VX_LIGHTDIREC:
            lightData.Type = D3DLIGHT_DIRECTIONAL;
            break;
        case VX_LIGHTPOINT:
            lightData.Type = D3DLIGHT_POINT;
            break;
        case VX_LIGHTSPOT:
            lightData.Type = D3DLIGHT_SPOT;
        default:
            return 0;
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

    /*if (data && Light < 0x80)
        m_CurrentLightData[Light] = *data;
    CKLightData lightData = *data;
    if (data->Type != VX_LIGHTPARA)
        lightData.Type = VX_LIGHTPOINT;
    else if (data->Type == VX_LIGHTSPOT)
    {
        lightData.InnerSpotCone = 3.14;
        if (lightData.OuterSpotCone < lightData.InnerSpotCone)
            lightData.OuterSpotCone = lightData.InnerSpotCone;
    }*/
    //ConvertAttenuationModelFromDX5(lightData.Attenuation0, lightData.Attenuation1, lightData.Attenuation2, data->Range);
    return SUCCEEDED(m_Device->SetLight(Light, &lightData));
    return 1;
}

BOOL CKDX9RasterizerContext::EnableLight(CKDWORD Light, BOOL Enable)
{
    return SUCCEEDED(m_Device->LightEnable(Light, Enable));
}

BOOL CKDX9RasterizerContext::SetMaterial(CKMaterialData* mat)
{
    if (mat)
        m_CurrentMaterialData = *mat;
    return SUCCEEDED(m_Device->SetMaterial((D3DMATERIAL9*)mat));
}

BOOL CKDX9RasterizerContext::SetViewport(CKViewportData* data)
{
    D3DVIEWPORT9 viewport{(DWORD)(data->ViewX), (DWORD)(data->ViewY), (DWORD)data->ViewWidth,
                          (DWORD)data->ViewHeight,     data->ViewZMin,       data->ViewZMax};
    return SUCCEEDED(m_Device->SetViewport(&viewport));
}

BOOL CKDX9RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix& Mat)
{
    CKDWORD UnityMatrixMask = 0;
    D3DTRANSFORMSTATETYPE D3DTs = (D3DTRANSFORMSTATETYPE)Type;
    switch (Type)
    {
        case VXMATRIX_WORLD:
            m_WorldMatrix = Mat;
            UnityMatrixMask = WORLD_TRANSFORM;
		    Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            m_MatrixUptodate &= 0xFE;
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
            UnityMatrixMask = 0x8 << (Type - 0x10);
            break;
        default:
            return FALSE;
    }
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
    }
    m_UnityMatrixMask &= ~UnityMatrixMask;
    return SUCCEEDED(m_Device->SetTransform(D3DTs, (const D3DMATRIX*)&Mat));
}

BOOL CKDX9RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
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

BOOL CKDX9RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD* Value)
{
    if (m_StateCache[State].Flag != 0)
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

BOOL CKDX9RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
#if LOGGING
    fprintf(stderr, "settexture %d %d\n", Texture, Stage);
#endif
    if (Texture >= m_Textures.Size())
        return 0;
    HRESULT hr = D3DERR_INVALIDCALL;
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc*>(m_Textures[Texture]);
    if (desc && desc->DxTexture)
    {
        hr = m_Device->SetTexture(Stage, desc->DxTexture);
        if (Stage == 0)
        {
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_COLOROP, 4)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, 2)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_COLORARG2, 1)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, 4)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, 2)));
        }
    } else
    {
        hr = m_Device->SetTexture(Stage, NULL);
        if (Stage == 0)
        {
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_COLOROP, 2)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, 0)));
            assert(SUCCEEDED(m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, 2)));
        }
    }
    
    return SUCCEEDED(hr);
}

BOOL CKDX9RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSV, Value);
            m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSW, Value);
            break;
        case CKRST_TSS_MAGFILTER:
            if (m_PresentInterval == 0)
            {
                LPDIRECT3DSTATEBLOCK9 block = m_TextureMagFilterStateBlocks[Value][Stage];
                if (block && SUCCEEDED(block->Apply()))
                    return TRUE;

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
                LPDIRECT3DSTATEBLOCK9 block = m_TextureMagFilterStateBlocks[Value][Stage];
                if (block && SUCCEEDED(block->Apply()))
                    return TRUE;
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
                if (block && SUCCEEDED(block->Apply()))
                    return TRUE;
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
                    m_Device->SetTextureStageState(Stage, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
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

                CKStageBlend *blend = static_cast<CKDX9Rasterizer *>(m_Driver->m_Owner)->m_BlendStages[Value];
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

BOOL CKDX9RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    if (VShaderIndex >= m_VertexShaders.Size())
        return 0;
    CKVertexShaderDesc* desc = m_VertexShaders[VShaderIndex];
    m_CurrentVertexShaderCache = VShaderIndex;
    m_CurrentVertexFormatCache = 0;
    return desc != NULL;
}

BOOL CKDX9RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    if (PShaderIndex >= m_PixelShaders.Size())
        return 0;
    CKDX9PixelShaderDesc* desc = static_cast<CKDX9PixelShaderDesc *>(m_PixelShaders[PShaderIndex]);
    if (desc)
        return SUCCEEDED(m_Device->GetPixelShader(&desc->DxShader));
    return 0;
}

BOOL CKDX9RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount)
{
    return SUCCEEDED(m_Device->SetPixelShaderConstantF(Register, static_cast<const float *>(Data), CstCount));
}

BOOL CKDX9RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount)
{
    return SUCCEEDED(m_Device->SetPixelShaderConstantF(Register, static_cast<const float *>(Data), CstCount));
}

BOOL CKDX9RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, WORD* indices, int indexcount,
	VxDrawPrimitiveData* data)
{
#if LOGGING
    fprintf(stderr, "drawprimitive ib %d\n", indexcount);
#endif
#if STEP
    if (step_mode)
    {
        this->BackToFront(false);
        _getch();
    }
#endif
    if (!m_SceneBegined)
        BeginScene();
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

    CKDWORD index = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    CKDX9VertexBufferDesc *vertexBufferDesc = static_cast<CKDX9VertexBufferDesc *>(
        m_VertexBuffers[index]);
    if (vertexBufferDesc == NULL)
        return 0;
    CKDWORD currentVCount = vertexBufferDesc->m_CurrentVCount;
    void *ppbData = NULL;
    HRESULT hr = D3DERR_INVALIDCALL;
    CKDWORD startIndex = 0;
    
    if (currentVCount + data->VertexCount <= vertexBufferDesc->m_MaxVertexCount)
    {
        hr = vertexBufferDesc->DxBuffer->Lock(vertexSize * currentVCount, vertexSize * data->VertexCount, &ppbData,
                                         D3DLOCK_NOOVERWRITE);
        startIndex = vertexBufferDesc->m_CurrentVCount;
        vertexBufferDesc->m_CurrentVCount += data->VertexCount;
    } else
    {
        hr = vertexBufferDesc->DxBuffer->Lock(0, vertexSize * data->VertexCount, &ppbData, D3DLOCK_DISCARD);
        vertexBufferDesc->m_CurrentVCount = data->VertexCount;
    }
    if (FAILED(hr))
        return 0;
    CKRSTLoadVertexBuffer(reinterpret_cast<CKBYTE *>(ppbData), vertexFormat, vertexSize, data);
    assert(SUCCEEDED(vertexBufferDesc->DxBuffer->Unlock()));
    return InternalDrawPrimitiveVB(pType, vertexBufferDesc, startIndex, data->VertexCount, indices, indexcount, clip);
}

BOOL CKDX9RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
	CKDWORD VertexCount, WORD* indices, int indexcount)
{
#if LOGGING
    fprintf(stderr, "drawprimitive vb %d %d\n", VertexCount, indexcount);
#endif
#if STEP
    if (step_mode)
        _getch();
#endif
    if (VertexBuffer >= m_VertexBuffers.Size())
        return 0;
    CKVertexBufferDesc* vertexBufferDesc = m_VertexBuffers[VertexBuffer];
    if (vertexBufferDesc == NULL)
        return 0;
    if (!m_SceneBegined)
        BeginScene();
    return InternalDrawPrimitiveVB(pType, static_cast<CKDX9VertexBufferDesc*>(vertexBufferDesc), StartIndex, VertexCount, indices, indexcount, TRUE);
}

BOOL CKDX9RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
	CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if LOGGING
    fprintf(stderr, "drawprimitive vbib %d %d\n", VertexCount, Indexcount);
#endif
#if STEP
    if (step_mode)
        _getch();
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
            Indexcount = Indexcount >> 1;
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

    m_Device->GetIndices(&indexBufferDesc->DxBuffer);

	return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, 0, 0, VertexCount, StartIndex, Indexcount));
}

BOOL CKDX9RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void* DesiredFormat)
{
    int result; // eax

    if (ObjIndex >= m_Textures.Size())
        return 0;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
            return 0;
            result = CreateSprite(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
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

void* CKDX9RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
	CKRST_LOCKFLAGS Lock)
{
    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return FALSE;

    void* pVertices = NULL;
    if (FAILED(vb->DxBuffer->Lock(StartVertex * vb->m_VertexSize, VertexCount * vb->m_VertexSize, &pVertices,
                                  Lock << 12)))
        return NULL;

    return pVertices;
}

BOOL CKDX9RasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    if (VB >= m_VertexBuffers.Size())
        return FALSE;

    CKDX9VertexBufferDesc *vb = static_cast<CKDX9VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vb || !vb->DxBuffer)
        return FALSE;

    return SUCCEEDED(vb->DxBuffer->Unlock());
}

BOOL CKDX9RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
#if LOGGING
    fprintf(stderr, "load texture %d %dx%d %d\n", Texture, SurfDesc.Width, SurfDesc.Height, miplevel);
#endif
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxTexture)
        return FALSE;
    if ((desc->Flags & (CKRST_TEXTURE_CUBEMAP | CKRST_TEXTURE_RENDERTARGET)) != 0)
        return TRUE;
    int actual_miplevel = (miplevel < 0) ? 0 : miplevel;
    D3DSURFACE_DESC SurfaceDesc;
    IDirect3DSurface9 *pSurface = NULL;
    IDirect3DSurface9 *pSurfaceLevel = NULL;

    desc->DxTexture->GetLevelDesc(actual_miplevel, &SurfaceDesc);
    if (SurfaceDesc.Format == D3DFMT_DXT1 || SurfaceDesc.Format == D3DFMT_DXT2 || SurfaceDesc.Format == D3DFMT_DXT3 ||
        SurfaceDesc.Format == D3DFMT_DXT4 || SurfaceDesc.Format == D3DFMT_DXT5)
    {
        desc->DxTexture->GetSurfaceLevel(actual_miplevel, &pSurfaceLevel);
        if (pSurfaceLevel)
        {
            RECT SrcRect{0, 0, SurfDesc.Height, SurfDesc.Width};
            D3DFORMAT format = VxPixelFormatToD3DFormat(VxImageDesc2PixelFormat(SurfDesc));
            D3DXLoadSurfaceFromMemory(pSurfaceLevel, NULL, NULL, SurfDesc.Image, format, SurfDesc.BytesPerLine, NULL,
                                      &SrcRect, 3, 0);

            CKDWORD MipMapCount = m_Textures[Texture]->MipMapCount;
            HRESULT hr;
            if (miplevel == -1 && MipMapCount > 0)
            {
                for (int i = 1; i < MipMapCount + 1; ++i)
                {
                    desc->DxTexture->GetSurfaceLevel(i, &pSurface);
                    hr = D3DXLoadSurfaceFromSurface(pSurface, NULL, NULL, pSurfaceLevel, NULL, NULL, 5, NULL);
                    pSurfaceLevel->Release();
                }
            }
            else
            {
                pSurface = pSurfaceLevel;
            }
            if (pSurface)
                pSurface->Release();
#if LOGGING
            if (FAILED(hr))
                fprintf(stderr, "LoadTexture (DXTC) failed with %x\n", hr);
#endif
            return SUCCEEDED(hr);
        }
        return 0;
    }
    VxImageDescEx src = SurfDesc;
    VxImageDescEx dst;
    if (miplevel != -1 || !desc->MipMapCount)
        pSurface = NULL;
    CKBYTE *image = NULL;
    if (pSurface)
    {
        image = m_Driver->m_Owner->AllocateObjects(SurfaceDesc.Width * SurfaceDesc.Height);
        if (SurfaceDesc.Width != src.Width || SurfaceDesc.Height != src.Height)
        {
            dst.Size = sizeof(VxImageDescEx);
            ZeroMemory(&dst.Flags, sizeof(VxImageDescEx) - sizeof(dst.Size));
            dst.Width = src.Width;
            dst.Height = src.Height;
            dst.BitsPerPixel = 32;
            dst.BytesPerLine = 4 * SurfDesc.Width;
            dst.AlphaMask = 0xFF000000;
            dst.RedMask = 0xFF0000;
            dst.GreenMask = 0xFF00;
            dst.BlueMask = 0xFF;
            dst.Image = image;
            VxDoBlit(src, dst);
            src = dst;
        }
    }
    D3DLOCKED_RECT LockRect;
    HRESULT hr = desc->DxTexture->LockRect(actual_miplevel, &LockRect, NULL, 0);
    if (FAILED(hr))
    {
#if LOGGING
        fprintf(stderr, "LoadTexture (Locking) failed with %x\n", hr);
#endif
        return FALSE;
    }
    LoadSurface(SurfaceDesc, LockRect, src);
    desc->DxTexture->UnlockRect(actual_miplevel);
    if (pSurface)
    {
        dst = src;
        for (int i = 1; i < desc->MipMapCount + 1; ++i)
        {
            VxGenerateMipMap(dst, image);
            dst.BytesPerLine = 4 * dst.Width;
            if (dst.Width > 1)
                dst.Width >>= 1;
            if (dst.Height > 1)
                dst.Height >>= 1;
            dst.Image = image;
            hr = desc->DxTexture->LockRect(i, &LockRect, NULL, NULL);
            if (FAILED(hr))
            {
#if LOGGING
                fprintf(stderr, "LoadTexture (Mipmap generation) failed with %x\n", hr);
#endif
                return FALSE;
            }
            LoadSurface(SurfaceDesc, LockRect, dst);
            desc->DxTexture->UnlockRect(i);
        }
    }
    return TRUE;
}

#include <cstdint>
#define SBYTEn(x, n) (*((int8_t *)&(x) + n))
#define LAST_IND(x, part_type) (sizeof(x) / sizeof(part_type) - 1)
#define LOW_IND(x, part_type) LAST_IND(x, part_type)
#define SLOBYTE(x) SBYTEn(x, LOW_IND(x, int8_t))
BOOL CKDX9RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect* Src, VxRect* Dest, CKRST_CUBEFACE Face)
{
#if LOGGING
    fprintf(stderr, "copy to texture %d (%f,%f,%f,%f) (%f,%f,%f,%f)\n", Texture,
        Src->left, Src->top, Src->right, Src->bottom,
        Dest->left, Dest->top, Dest->right, Dest->bottom);
#endif
    if (Texture >= m_Textures.Size())
        return 0;
    CKDX9TextureDesc *desc = static_cast<CKDX9TextureDesc *>(m_Textures[Texture]);
    if (!desc || !desc->DxTexture)
        return 0;
    tagRECT destRect;
    if (Dest)
        SetRect(&destRect, Dest->left, Dest->top, Dest->right, Dest->bottom);
    else
        SetRect(&destRect, 0, 0, desc->Format.Width, desc->Format.Height);

    tagRECT srcRect;
    if (Src)
        SetRect(&srcRect, Src->left, Src->top, Src->right, Src->bottom);
    else
        SetRect(&srcRect, 0, 0, desc->Format.Width, desc->Format.Height);

    IDirect3DSurface9 *backBuffer = NULL;
    IDirect3DSurface9 *textureSurface = NULL;
    assert(SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)));
    assert(SUCCEEDED(desc->DxTexture->GetSurfaceLevel(0, &textureSurface)));
    POINT pt{destRect.left, destRect.top};

    HRESULT hr = E_FAIL;
    if (backBuffer && textureSurface)
    {
        assert(SUCCEEDED(hr = m_Device->UpdateSurface(textureSurface, &srcRect, backBuffer, &pt)));
        textureSurface->Release();
        textureSurface = NULL;
    }
    else if (textureSurface)
    {
        textureSurface->Release();
    }
    if (FAILED(hr) && SLOBYTE(desc->Flags) < 0)
    {
        desc->DxTexture->Release();
        desc->DxTexture = NULL;

        if (SUCCEEDED(m_Device->CreateTexture(
            desc->Format.Width, desc->Format.Height, 1,
            D3DUSAGE_RENDERTARGET, m_PresentParams.BackBufferFormat,
            D3DPOOL_DEFAULT, &desc->DxTexture, NULL)))
        {
            D3DFormatToTextureDesc(m_PresentParams.BackBufferFormat, desc);
            desc->Flags &= 0x7F;
            desc->Flags |= (CKRST_TEXTURE_RENDERTARGET | CKRST_TEXTURE_VALID);
            desc->DxTexture->GetSurfaceLevel(0, &textureSurface);
            assert(SUCCEEDED(hr = m_Device->UpdateSurface(backBuffer, &srcRect, textureSurface, &pt)));
            if (textureSurface)
                textureSurface->Release();
            if (backBuffer)
                backBuffer->Release();
            return SUCCEEDED(hr);
        }
    }

    return 0;
}

BOOL CKDX9RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect* src, VxRect* dst)
{
	return CKRasterizerContext::DrawSprite(Sprite, src, dst);
}

int CKDX9RasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    D3DSURFACE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    IDirect3DSurface9 *surface = NULL;
    int v33 = 0;

    switch (buffer)
    {
        case VXBUFFER_BACKBUFFER:
        {
            assert(SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface)));
            if (!surface)
                return 0;
            assert(SUCCEEDED(surface->GetDesc(&desc)));
            VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(desc.Format);
            VxPixelFormat2ImageDesc(vxpf, img_desc);
            break;
        }
        case VXBUFFER_ZBUFFER:
        {
            assert(SUCCEEDED(m_Device->GetDepthStencilSurface(&surface)));
            if (!surface)
                return 0;
            assert(SUCCEEDED(surface->GetDesc(&desc)));
            img_desc.BitsPerPixel = 32;
            img_desc.AlphaMask = 0;
            img_desc.BlueMask = 0xFF;
            img_desc.GreenMask = 0xFF00;
            img_desc.RedMask = 0xFF0000;
            switch (desc.Format)
            {
                case D3DFMT_D16_LOCKABLE:
                case D3DFMT_D15S1:
                case D3DFMT_D16:
                    v33 = 2;
                    break;
                case D3DFMT_D32:
                case D3DFMT_D24S8:
                case D3DFMT_D24X8:
                case D3DFMT_D24X4S4:
                    v33 = 4;
                    break;
                default:
                    goto out;
            }
        }
        case VXBUFFER_STENCILBUFFER:
        {
            assert(SUCCEEDED(m_Device->GetDepthStencilSurface(&surface)));
            if (!surface)
                return 0;
            assert(SUCCEEDED(surface->GetDesc(&desc)));
            D3DFORMAT D3DFormat = desc.Format;
            img_desc.BitsPerPixel = 32;
            img_desc.AlphaMask = 0;
            img_desc.BlueMask = 0xFF;
            img_desc.GreenMask = 0xFF00;
            img_desc.RedMask = 0xFF0000;
            if (D3DFormat != D3DFMT_D15S1 && D3DFormat != D3DFMT_D24S8 && D3DFormat != D3DFMT_D24X4S4)
            {
                surface->Release();
                return 0;
            }
            break;
        }
    }
    out:
    UINT right, left, top, bottom;
    if (rect)
    {
        right = (rect->right > desc.Width) ? desc.Width : rect->right;
        left = (rect->left < 0) ? 0 : rect->left;
        top = (rect->top < 0) ? 0 : rect->top;
        bottom = (rect->bottom > desc.Height) ? desc.Height : rect->bottom;
    } else
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
    img_desc.BytesPerLine = width * img_desc.BitsPerPixel / 8;
    if (width != 0)
    {
        IDirect3DSurface9 *ImageSurface = NULL;
        D3DLOCKED_RECT LockedRect;
        if (FAILED(m_Device->CreateOffscreenPlainSurface(width, height, desc.Format,
            D3DPOOL_SCRATCH ,&ImageSurface, NULL)))
        {
            ImageSurface = surface;
            surface->AddRef();
        }
        else if (FAILED(m_Device->UpdateSurface(surface, NULL, ImageSurface, NULL)) || 
            FAILED(ImageSurface->LockRect(&LockedRect, NULL, D3DLOCK_READONLY)))
        {
            ImageSurface->Release();
            surface->Release();
            return 0;
        }

        BYTE *pBits = static_cast<BYTE *>(LockedRect.pBits);
        BYTE *imgBuffer = img_desc.Image;
        if (img_desc.BitsPerPixel == 32)
        {
            for (UINT i = 0; i < width; ++i)
            {
                BYTE *cur = &pBits[i * 4];
                *reinterpret_cast<DWORD *>(imgBuffer) = *reinterpret_cast<DWORD *>(cur);
                imgBuffer += 4;
            }
        }
        assert(SUCCEEDED(ImageSurface->UnlockRect()));
        assert(SUCCEEDED(ImageSurface->Release()));
    }
    return (buffer == VXBUFFER_BACKBUFFER);
}

int CKDX9RasterizerContext::CopyFromMemoryBuffer(CKRECT* rect, VXBUFFER_TYPE buffer, const VxImageDescEx& img_desc)
{
	return CKRasterizerContext::CopyFromMemoryBuffer(rect, buffer, img_desc);
}

BOOL CKDX9RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face,
	BOOL GenerateMipMap)
{
	return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face, GenerateMipMap);
}

BOOL CKDX9RasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane& PlaneEquation)
{
    return SUCCEEDED(m_Device->SetClipPlane(ClipPlaneIndex, (const float *)&PlaneEquation));
}

BOOL CKDX9RasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane& PlaneEquation)
{
    return SUCCEEDED(m_Device->GetClipPlane(ClipPlaneIndex, (float *)&PlaneEquation));
}

void* CKDX9RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
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

BOOL CKDX9RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB >= m_IndexBuffers.Size())
        return FALSE;

    CKDX9IndexBufferDesc *ib = static_cast<CKDX9IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ib || !ib->DxBuffer)
        return FALSE;

    return SUCCEEDED(ib->DxBuffer->Unlock());
}

BOOL CKDX9RasterizerContext::CreateTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param)
{
	return FALSE;

}

void CKDX9RasterizerContext::UpdateDirectXData()
{
    IDirect3DSurface9 *pBackBuffer = NULL, *pZStencilSurface = NULL;
    assert(SUCCEEDED(m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)));
    assert(SUCCEEDED(m_Device->GetDepthStencilSurface(&pZStencilSurface)));
    m_DirectXData.D3DDevice = m_Device;
    m_DirectXData.DxVersion = D3DX_VERSION;
    m_DirectXData.D3DViewport = NULL;
    m_DirectXData.DDBackBuffer = pBackBuffer;
    m_DirectXData.DDPrimaryBuffer = NULL;
    m_DirectXData.DDZBuffer = pZStencilSurface;
    m_DirectXData.DirectDraw = NULL;
    m_DirectXData.Direct3D = m_Owner->m_D3D9;
    m_DirectXData.DDClipper = NULL;
    if (pZStencilSurface)
        assert(SUCCEEDED(pZStencilSurface->Release()));
    if (pBackBuffer)
        assert(SUCCEEDED(pBackBuffer->Release()));
}

BOOL CKDX9RasterizerContext::InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc* VB,
	CKDWORD StartIndex, CKDWORD VertexCount, WORD* indices, int indexcount, BOOL Clip)
{
    int ibstart = 0;
    if (indices)
    {
        CKDX9IndexBufferDesc* desc = this->m_IndexBuffer[Clip];
        int length = indexcount + 100;
        HRESULT hr = D3DERR_INVALIDCALL;
        if (!desc || desc->m_MaxIndexCount < indexcount || !desc->DxBuffer)
        {
            if (desc)
            {
                if (desc->DxBuffer)
                    desc->DxBuffer->Release();
                delete desc;
                desc = NULL;
            }
            if (length <= 10000)
                length = 10000;
            desc = new CKDX9IndexBufferDesc;
            desc->DxBuffer = NULL;
            desc->m_MaxIndexCount = 0;
            desc->m_CurrentICount = 0;
            desc->m_Flags = 0;

            DWORD usage = (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY |
                           (m_SoftwareVertexProcessing ? D3DUSAGE_SOFTWAREPROCESSING : 0));
            assert(SUCCEEDED(hr = m_Device->CreateIndexBuffer(2 * length, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                                              &desc->DxBuffer, NULL)));
            desc->m_MaxIndexCount = length;
            m_IndexBuffer[Clip] = desc;
        }
        void *pbData = NULL;
        if (indexcount + desc->m_CurrentICount <= desc->m_MaxIndexCount)
        {
            desc->DxBuffer->Lock(2 * desc->m_CurrentICount, 2 * indexcount, &pbData, D3DLOCK_NOOVERWRITE);
            ibstart = desc->m_CurrentICount;
            desc->m_CurrentICount += indexcount;
        } else
        {
            desc->DxBuffer->Lock(0, 2 * indexcount, &pbData, D3DLOCK_DISCARD);
            desc->m_CurrentICount = indexcount;
        }
        if (pbData)
        {
            memcpy(pbData, indices, 2 * indexcount);
        }
        desc->DxBuffer->Unlock();
    }
    SetupStreams(VB->DxBuffer, VB->m_VertexFormat, VB->m_VertexSize);
    int primCount = indexcount;
    if (indexcount == 0)
        primCount = VertexCount;
    switch (pType)
    {
        case VX_LINELIST:
            primCount /= 2;
            break;
        case VX_LINESTRIP:
            primCount--;
            break;
        case VX_TRIANGLELIST:
            primCount /= 3;
            break;
        case VX_TRIANGLESTRIP:
        case VX_TRIANGLEFAN:
            primCount -= 2;
            break;
        default:
            break;
    }
    if (!indices || pType == VX_POINTLIST)
    {
        HRESULT hr = m_Device->DrawPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, primCount);
        return SUCCEEDED(hr);
    }
    if (FAILED(m_Device->SetIndices(m_IndexBuffer[Clip]->DxBuffer)))
        return 0;
    // baseVertexIndex == 0?
    return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, 0, VertexCount, ibstart, primCount));
}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize)
{
    // TODO: Utilize cache
    assert(SUCCEEDED(m_Device->SetFVF(VFormat)));
    assert(SUCCEEDED(m_Device->SetStreamSource(0, Buffer, 0, VSize)));

    //if (m_CurrentVertexShaderCache)
    //{
    //    CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(m_VertexShaders[m_CurrentVertexShaderCache]);
    //    if (desc)
    //    {
    //        assert(SUCCEEDED(m_Device->SetVertexShader(NULL)));
    //        assert(SUCCEEDED(m_Device->SetFVF(VFormat)));
    //        desc->DxShader = NULL;
    //    }
    //} else
    //{
    //    if (VFormat != m_CurrentVertexFormatCache)
    //    {
    //        m_CurrentVertexFormatCache = VFormat;
    //        assert(SUCCEEDED(m_Device->SetFVF(VFormat)));
    //    }
    //}
    //if (Buffer != m_CurrentVertexBufferCache || m_CurrentVertexSizeCache != VSize)
    //{
    //    //assert(SUCCEEDED(m_Device->GetStreamSource(0, &Buffer, &offset, NULL)));
    //    assert(SUCCEEDED(m_Device->SetStreamSource(0, Buffer, 0, VSize)));
    //    m_CurrentVertexBufferCache = Buffer;
    //    m_CurrentVertexSizeCache = VSize;
    //}
}

BOOL CKDX9RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc* DesiredFormat)
{
#if LOGGING
    fprintf(stderr, "create texture %d %dx%d\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height);
#endif
    if (Texture >= m_Textures.Size())
        return FALSE;
    if (m_Textures[Texture])
        return TRUE;
    CKDX9TextureDesc *desc = new CKDX9TextureDesc();
    m_Textures[Texture] = desc;
    auto width = DesiredFormat->Format.Width;
    auto height = DesiredFormat->Format.Height;
    auto miplvl = DesiredFormat->MipMapCount;
    auto fmt = VxPixelFormatToD3DFormat(VxImageDesc2PixelFormat(DesiredFormat->Format));
    return SUCCEEDED(m_Device->CreateTexture(width, height, miplvl, D3DUSAGE_DYNAMIC, fmt, D3DPOOL_DEFAULT, &(desc->DxTexture), NULL));
}

BOOL CKDX9RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc* DesiredFormat)
{
#if LOGGING
    fprintf(stderr, "create vertex shader %d\n", VShader);
#endif
    if (VShader >= m_VertexShaders.Size() || !DesiredFormat)
        return 0;
    CKVertexShaderDesc *shader = m_VertexShaders[VShader];
    if (DesiredFormat == shader)
    {
        CKDX9VertexShaderDesc *desc = static_cast<CKDX9VertexShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat);
    }

    if (shader)
        delete shader;
    m_VertexShaders[VShader] = NULL;
    CKDX9VertexShaderDesc* desc = new CKDX9VertexShaderDesc;
    if (!desc)
        return 0;
    desc->m_Function = NULL;
    desc->m_FunctionData.Clear();
    desc->DxShader = NULL;
    desc->Owner = NULL;
    if (desc->Create(this, DesiredFormat))
    {
        m_VertexShaders[VShader] = desc;
        return 1;
    }
    return 0;
}

BOOL CKDX9RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc* DesiredFormat)
{
#if LOGGING
    fprintf(stderr, "create pixel shader %d\n", PShader);
#endif
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return 0;
    if (DesiredFormat == m_PixelShaders[PShader])
    {
        CKDX9PixelShaderDesc *desc = static_cast<CKDX9PixelShaderDesc *>(DesiredFormat);
        return desc->Create(this, DesiredFormat->m_Function);
    }
    if (m_PixelShaders[PShader])
        delete m_PixelShaders[PShader];
    CKDX9PixelShaderDesc *desc = new CKDX9PixelShaderDesc;
    if (!desc)
        return 0;
    desc->m_Function = NULL;
    desc->DxShader = NULL;
    desc->Owner = NULL;
    if (desc->Create(this, DesiredFormat->m_Function))
    {
        m_PixelShaders[PShader] = desc;
        return 1;
    }
    return 0;
}

BOOL CKDX9RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc* DesiredFormat)
{
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return 0;
    DWORD fvf = D3DFVF_XYZB2;
    if ((DesiredFormat->m_Flags & CKRST_VB_DYNAMIC) != 0)
        fvf |= D3DFVF_TEX2;
    IDirect3DVertexBuffer9 *vb = NULL;
    if (FAILED(m_Device->CreateVertexBuffer(DesiredFormat->m_MaxVertexCount * DesiredFormat->m_VertexSize, fvf,
                                            DesiredFormat->m_VertexFormat, D3DPOOL_DEFAULT, &vb, NULL)))
        return 0;
    if (m_VertexBuffers[VB] == DesiredFormat)
    {
        CKDX9VertexBufferDesc *dx9Desc = static_cast<CKDX9VertexBufferDesc *>(DesiredFormat);
        dx9Desc->DxBuffer = vb;
        return 1;
    }
    if (m_VertexBuffers[VB])
        delete m_VertexBuffers[VB];
    CKDX9VertexBufferDesc *desc = new CKDX9VertexBufferDesc;
    if (!desc)
        return 0;
    desc->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    desc->m_VertexSize = DesiredFormat->m_VertexSize;
    desc->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount;
    desc->m_VertexFormat = DesiredFormat->m_VertexFormat;
    desc->m_Flags = DesiredFormat->m_Flags;
    desc->DxBuffer = vb;
    desc->m_Flags |= 1;
    m_VertexBuffers[VB] = desc;
    return 1;
}

BOOL CKDX9RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc* DesiredFormat)
{
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return 0;
    DWORD usage = D3DUSAGE_WRITEONLY;
    if ((DesiredFormat->m_Flags & CKRST_VB_DYNAMIC) != 0)
        usage |= D3DUSAGE_DYNAMIC;
    IDirect3DIndexBuffer9 *buffer;
    if (FAILED(m_Device->CreateIndexBuffer(2 * DesiredFormat->m_MaxIndexCount, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                           &buffer, NULL)))
        return 0;
    if (DesiredFormat == m_IndexBuffer[IB])
    {
        CKDX9IndexBufferDesc *desc = static_cast<CKDX9IndexBufferDesc *>(DesiredFormat);
        desc->DxBuffer = buffer;
        desc->m_Flags |= 1;
        return 1;
    }
    if (m_IndexBuffer[IB])
        delete m_IndexBuffer[IB];
    CKDX9IndexBufferDesc *desc = new CKDX9IndexBufferDesc;
    if (!desc)
        return 0;
    desc->m_CurrentICount = DesiredFormat->m_CurrentICount;
    desc->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount;
    desc->m_Flags = DesiredFormat->m_Flags;
    desc->DxBuffer = buffer;
    desc->m_Flags |= 1;
    m_IndexBuffer[IB] = desc;
    return 1;
}

void CKDX9RasterizerContext::FlushCaches()
{
    FlushRenderStateCache();

    m_InverseWinding = FALSE;

    memset(m_TextureMinFilterStateBlocks, NULL, sizeof(m_TextureMinFilterStateBlocks));
    memset(m_TextureMagFilterStateBlocks, NULL, sizeof(m_TextureMagFilterStateBlocks));
    memset(m_TextureMapBlendStateBlocks, NULL, sizeof(m_TextureMapBlendStateBlocks));

    if (m_Device)
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                m_Device->BeginStateBlock();
                SetTextureStageState(i, CKRST_TSS_MINFILTER, j + 1);
                m_Device->EndStateBlock(&m_TextureMinFilterStateBlocks[j][i]);
                
                m_Device->BeginStateBlock();
                SetTextureStageState(i, CKRST_TSS_MAGFILTER, j + 1);
                m_Device->EndStateBlock(&m_TextureMagFilterStateBlocks[j][i]);

                for (int k = 0; k < 10; k++)
                {
                    m_Device->BeginStateBlock();
                    SetTextureStageState(i, CKRST_TSS_TEXTUREMAPBLEND, k + 1);
                    m_Device->EndStateBlock(&m_TextureMapBlendStateBlocks[k][i]);
                }
            }
        }
    }
}

void CKDX9RasterizerContext::FlushNonManagedObjects()
{
    if (m_Device)
    {
        /*IDirect3DIndexBuffer9 *ib;
        assert(SUCCEEDED(m_Device->GetIndices(&ib)));
        assert(SUCCEEDED(m_Device->GetStreamSource(0, NULL, NULL, NULL)));
        */
        if (m_DefaultBackBuffer && m_DefaultDepthBuffer)
        {
            assert(SUCCEEDED(m_Device->SetRenderTarget(0, m_DefaultBackBuffer)));
            assert(SUCCEEDED(m_Device->SetDepthStencilSurface(m_DefaultDepthBuffer)));
            m_DefaultBackBuffer->Release();
            m_DefaultBackBuffer = NULL;
            m_DefaultDepthBuffer->Release();
            m_DefaultDepthBuffer = NULL;
        }
    }
    
    for (int i = 0; i < m_Textures.Size(); ++i)
    {
        if (m_Textures[i] && (m_Textures[i]->Flags & CKRST_TEXTURE_MANAGED) == 0)
            delete m_Textures[i];
    }
    ReleaseTempZBuffers();
    FlushObjects(60);
    return ReleaseIndexBuffers();
}

void CKDX9RasterizerContext::ReleaseStateBlocks()
{
    if (m_Device)
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                m_TextureMinFilterStateBlocks[j][i]->Release();
                m_TextureMagFilterStateBlocks[j][i]->Release();
            }

            for (int k = 0; k < 10; k++)
            {
                m_TextureMapBlendStateBlocks[k][i]->Release();
            }
        }

        memset(m_TextureMinFilterStateBlocks, NULL, sizeof(m_TextureMinFilterStateBlocks));
        memset(m_TextureMagFilterStateBlocks, NULL, sizeof(m_TextureMagFilterStateBlocks));
        memset(m_TextureMapBlendStateBlocks, NULL, sizeof(m_TextureMapBlendStateBlocks));
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

CKDWORD CKDX9RasterizerContext::DX9PresentInterval(DWORD PresentInterval)
{
	return 0;
}

BOOL CKDX9RasterizerContext::LoadSurface(const D3DSURFACE_DESC& ddsd, const D3DLOCKED_RECT& LockRect,
	const VxImageDescEx& SurfDesc)
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
        mov		eax,data
        bsr		eax,eax
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
