#include "CKDX9Rasterizer.h"

CKDX9RasterizerContext::CKDX9RasterizerContext(CKDX9RasterizerDriver* Driver) :
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
	m_TempZBuffers{},
	m_Owner(static_cast<CKDX9Rasterizer*>(Driver->m_Owner))
{
    m_Driver = Driver;
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
    CKDX9RasterizerDriver *Driver = static_cast<CKDX9RasterizerDriver *>(m_Driver);
	memset(&m_PresentParams, 0, sizeof(m_PresentParams));
	m_PresentParams.hDeviceWindow = (HWND) Window;
	m_PresentParams.BackBufferWidth = Width;
	m_PresentParams.BackBufferHeight = Height;
	m_PresentParams.BackBufferCount = 1;
	m_PresentParams.Windowed = !Fullscreen;
	m_PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	m_PresentParams.EnableAutoDepthStencil = TRUE;
	m_PresentParams.FullScreen_RefreshRateInHz = Fullscreen ? RefreshRate : 0;
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
        m_Width = desc.Width;
        m_Height = desc.Height;
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

BOOL CKDX9RasterizerContext::EndScene()
{
    if (!m_SceneBegined)
        return 1;
    HRESULT hr = m_Device->EndScene();
    m_SceneBegined = 0;
    return SUCCEEDED(hr);
}

BOOL CKDX9RasterizerContext::SetLight(CKDWORD Light, CKLightData* data)
{
    if (data && Light < 0x80)
        m_CurrentLightData[Light] = *data;
    CKLightData lightData = *data;
    if (data->Type != VX_LIGHTPARA)
        lightData.Type = VX_LIGHTPOINT;
    else if (data->Type == VX_LIGHTSPOT)
    {
        lightData.InnerSpotCone = 3.14;
        if (lightData.OuterSpotCone < lightData.InnerSpotCone)
            lightData.OuterSpotCone = lightData.InnerSpotCone;
    }
    ConvertAttenuationModelFromDX5(lightData.Attenuation0, lightData.Attenuation1, lightData.Attenuation2, data->Range);
    return SUCCEEDED(m_Device->SetLight(Light, (D3DLIGHT9*) &lightData));
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
    return SUCCEEDED(m_Device->SetRenderState((D3DRENDERSTATETYPE)State, Value));
}

BOOL CKDX9RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD* Value)
{
    return SUCCEEDED(m_Device->GetRenderState((D3DRENDERSTATETYPE)State, Value));
}

BOOL CKDX9RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
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
    int v4; // ebx
    int v6; // edi
    CKDWORD v7; // ebp
    CKDWORD v8; // edx
    BOOL result; // eax
    int v10; // edi
    CKDWORD v11; // ebp
    DWORD v12; // ecx
    int v13; // edi
    CKDWORD v14; // ebp
    CKDWORD m_PresentInterval; // eax
    int v16; // edi
    int v18; // edi
    DWORD v19; // edi
    int v20; // ebp
    HRESULT v21; // ebx
    int v22; // [esp+4Ch] [ebp-10h]

    v4 = -1;
    switch (Tss)
    {
        case CKRST_TSS_ADDRESS:
            v19 = Value;
            v20 = Stage;
            v21 = this->m_Device->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
            this->m_Device->SetSamplerState(v20, D3DSAMP_ADDRESSV, v19);
            this->m_Device->SetSamplerState(v20, D3DSAMP_ADDRESSW, v19);
            result = v21 >= 0;
            break;
        case CKRST_TSS_MAGFILTER:
            if (this->m_PresentInterval)
            {
                this->m_Device->SetSamplerState(Stage, D3DSAMP_MAGFILTER, 1);
                result = 0;
            }
            else
            {
                v10 = Stage;
                v11 = Value;
                v12 = this->m_TextureMagFilterStateBlocks[Value][Stage];
                if (v12)
                {
                    /*v4 = this->m_Device->ApplyStateBlock(v12);
                    if (v4 >= 0)*/
                        goto LABEL_45;
                }
                switch (v11)
                {
                    case 1u:
                    case 3u:
                        this->m_Device->SetSamplerState(v10, D3DSAMP_MAGFILTER, 1);
                        result = v4 >= 0;
                        break;
                    case 2u:
                    case 4u:
                    case 5u:
                    case 6u:
                        this->m_Device->SetSamplerState(v10, D3DSAMP_MAGFILTER, 2);
                        result = v4 >= 0;
                        break;
                    case 7u:
                        this->m_Device->SetSamplerState(v10, D3DSAMP_MAXANISOTROPY, 2);
                        this->m_Device->SetSamplerState(v10, D3DSAMP_MAGFILTER, 3);
                        result = v4 >= 0;
                        break;
                    default:
                        goto LABEL_45;
                }
            }
            break;
        case CKRST_TSS_MINFILTER:
            v13 = Stage;
            v14 = Value;
            /*if (this->m_TextureMinFilterStateBlocks[Value][Stage] && !this->m_PresentInterval &&
                !this->m_CurrentPresentInterval)
            {
                v4 = this->m_Device->ApplyStateBlock(this->m_TextureMinFilterStateBlocks[Value][Stage]);
            }*/
            m_PresentInterval = this->m_PresentInterval;
            if (!m_PresentInterval && !this->m_CurrentPresentInterval && v4 >= 0)
                goto LABEL_45;
            switch (v14)
            {
                case 1u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER, 1);
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER, 0);
                    result = v4 >= 0;
                    break;
                case 2u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER,
                                                         2 - (m_PresentInterval != 0));
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER, 0);
                    result = v4 >= 0;
                    break;
                case 3u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER, 1);
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER,
                                                         this->m_CurrentPresentInterval == 0);
                    result = v4 >= 0;
                    break;
                case 4u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER,
                                                         2 - (m_PresentInterval != 0));
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER,
                                                         this->m_CurrentPresentInterval == 0);
                    result = v4 >= 0;
                    break;
                case 5u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER, 1);
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER,
                                                         this->m_CurrentPresentInterval != 0 ? 0 : 2);
                    result = v4 >= 0;
                    break;
                case 6u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER,
                                                         2 - (m_PresentInterval != 0));
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MIPFILTER,
                                                         this->m_CurrentPresentInterval != 0 ? 0 : 2);
                    result = v4 >= 0;
                    break;
                case 7u:
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MINFILTER,
                                                         2 * (m_PresentInterval == 0) + 1);
                    this->m_Device->SetSamplerState(v13, D3DSAMP_MAXANISOTROPY, 2);
                    result = v4 >= 0;
                    break;
                default:
                    goto LABEL_45;
            }
            break;
        case CKRST_TSS_TEXTUREMAPBLEND:
            v6 = Stage;
            v7 = Value;
            v8 = Stage + 8 * Value;
            if (this->m_TextureMapBlendStateBlocks[v8])
            {/*
                v4 = this->m_Device->ApplyStateBlock(this->m_TextureMapBlendStateBlocks[v8]);
                if (v4 >= 0)*/
                    goto LABEL_45;
            }
            switch (v7)
            {
                case 1u:
                case 7u:
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLOROP, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAOP, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAARG1, 2);
                    return 1;
                case 2u:
                case 4u:
                case 6u:
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLOROP, 4);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG2, 1);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAOP, 4);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAARG2, 1);
                    return 1;
                case 3u:
                case 5u:
                    m_Device->SetTextureStageState(v6, D3DTSS_COLOROP, 13);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG2, 1);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAOP, 2);
                    goto LABEL_10;
                case 8u:
                    m_Device->SetTextureStageState(v6, D3DTSS_COLOROP, 7);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG2, 1);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAOP, 2);
                    goto LABEL_10;
                case 9u:
                    m_Device->SetTextureStageState(v6, D3DTSS_COLORARG1, 2);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLOROP, 24);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_COLORARG2, 3);
                    this->m_Device->SetTextureStageState(v6, D3DTSS_ALPHAOP, 2);
                LABEL_10:
                    //m_Device->SetTextureStageState(v6, D3DTSS_ALPHAARG1, v22);
                    break;
                default:
                    break;
            }
            result = 1;
            break;
        case CKRST_TSS_STAGEBLEND:
            if (Value <= 0xFF)
            {
                if (Value)
                {
                    CKStageBlend* v17 = static_cast<CKDX9Rasterizer*>(m_Driver->m_Owner)->m_BlendStages[Value];
                    if (v17)
                    {
                        v18 = Stage;
                        this->m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, v17->Cop);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_COLORARG1, v17->Carg1);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_COLORARG2, v17->Carg2);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_ALPHAOP, v17->Aop);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_ALPHAARG1, v17->Aarg1);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_ALPHAARG2, v17->Aarg2);
                        v4 = this->m_Device->ValidateDevice((DWORD *)&Stage);
                        if (v4 >= 0)
                            goto LABEL_45;
                        this->m_Device->SetTextureStageState(v18, D3DTSS_COLOROP, 1);
                        this->m_Device->SetTextureStageState(v18, D3DTSS_ALPHAOP, 1);
                        result = v4 >= 0;
                    }
                    else
                    {
                        result = 0;
                    }
                }
                else
                {
                    v16 = Stage;
                    this->m_Device->SetTextureStageState(Stage, D3DTSS_COLOROP, 1);
                    this->m_Device->SetTextureStageState(v16, D3DTSS_ALPHAOP, 1);
                    result = 1;
                }
            }
            else
            {
                result = 0;
            }
            break;
        default:
            v4 = this->m_Device->SetTextureStageState(Stage, (D3DTEXTURESTAGESTATETYPE)Tss, Value);
        LABEL_45:
            result = v4 >= 0;
            break;
    }
    return result;
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
    if (!m_SceneBegined)
        BeginScene();
    CKBOOL clip = 0;
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    if ((data->Flags & CKRST_DP_DOCLIP) != 0)
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

    if (!CreateVertexBuffer(index, vertexBufferDesc))
        return 0;
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
    vertexBufferDesc->DxBuffer->Unlock();
    return InternalDrawPrimitiveVB(pType, vertexBufferDesc, startIndex, data->VertexCount, indices, indexcount, clip);
}

BOOL CKDX9RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
	CKDWORD VertexCount, WORD* indices, int indexcount)
{
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
	return CKRasterizerContext::DrawPrimitiveVBIB(pType, VB, IB, MinVIndex, VertexCount, StartIndex, Indexcount);
}

BOOL CKDX9RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void* DesiredFormat)
{
	return CKRasterizerContext::CreateObject(ObjIndex, Type, DesiredFormat);
}

void* CKDX9RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
	CKRST_LOCKFLAGS Lock)
{
	return CKRasterizerContext::LockVertexBuffer(VB, StartVertex, VertexCount, Lock);
}

BOOL CKDX9RasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
	return CKRasterizerContext::UnlockVertexBuffer(VB);
}

BOOL CKDX9RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, int miplevel)
{
	return CKRasterizerContext::LoadTexture(Texture, SurfDesc, miplevel);
}

BOOL CKDX9RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect* Src, VxRect* Dest, CKRST_CUBEFACE Face)
{
	return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}

BOOL CKDX9RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect* src, VxRect* dst)
{
	return CKRasterizerContext::DrawSprite(Sprite, src, dst);
}

int CKDX9RasterizerContext::CopyToMemoryBuffer(CKRECT* rect, VXBUFFER_TYPE buffer, VxImageDescEx& img_desc)
{
	return CKRasterizerContext::CopyToMemoryBuffer(rect, buffer, img_desc);
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
	return CKRasterizerContext::SetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

BOOL CKDX9RasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane& PlaneEquation)
{
	return CKRasterizerContext::GetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

void* CKDX9RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
	return CKRasterizerContext::LockIndexBuffer(IB, StartIndex, IndexCount, Lock);
}

BOOL CKDX9RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
	return CKRasterizerContext::UnlockIndexBuffer(IB);
}

BOOL CKDX9RasterizerContext::LockTextureVideoMemory(CKDWORD Texture, VxImageDescEx& Desc, int MipLevel,
	VX_LOCKFLAGS Flags)
{
	return FALSE;
}

BOOL CKDX9RasterizerContext::UnlockTextureVideoMemory(CKDWORD Texture, int MipLevel)
{
	return FALSE;

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
    if (indices)
    {
        CKDX9IndexBufferDesc* desc = this->m_IndexBuffer[Clip];
        int length = indexcount + 100;
        if (!desc || desc->m_MaxIndexCount < indexcount)
        {
            if (length <= 10000)
                length = 10000;
            CKDX9IndexBufferDesc ibDesc;
            ibDesc.DxBuffer = NULL;
            ibDesc.m_MaxIndexCount = 0;
            ibDesc.m_CurrentICount = 0;
            ibDesc.m_Flags = 0;
            desc = &ibDesc;

            DWORD usage = (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY |
                           (m_SoftwareVertexProcessing ? D3DUSAGE_SOFTWAREPROCESSING : 0));
            if (FAILED(m_Device->CreateIndexBuffer(2 * length, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &desc->DxBuffer, NULL)))
                // TODO: Log
                return 0;
            desc->m_MaxIndexCount = length;
            m_IndexBuffer[Clip] = desc;
        }
        void *pbData = NULL;
        if (indexcount + desc->m_CurrentICount <= desc->m_MaxIndexCount)
        {
            desc->DxBuffer->Lock(2 * desc->m_CurrentICount, 2 * indexcount, &pbData, D3DLOCK_NOOVERWRITE);
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
        HRESULT hr = m_Device->DrawPrimitive(D3DPT_POINTLIST, StartIndex, primCount);
        return SUCCEEDED(hr);
    }
    if (FAILED(m_Device->GetIndices(&m_IndexBuffer[Clip]->DxBuffer)))
        return 0;
    // baseVertexIndex == 0?
    return SUCCEEDED(m_Device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)pType, StartIndex, 0, VertexCount, startIndex, primCount));
}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize)
{
    if (m_CurrentVertexShaderCache)
    {
        int index = index = ((int)(VFormat & 0x14 | ((int)VFormat >> 3) & 0x1F8) >> 2) + 1;
        CKDX9VertexShaderDesc *shaderDesc =
            static_cast<CKDX9VertexShaderDesc *>(m_VertexShaders[m_CurrentVertexShaderCache]);
        if (shaderDesc)
        {
            if (!shaderDesc->DxShader)
            {
               // D3DXDeclaratorFromFVF()
                
            }
        }
    }
}

BOOL CKDX9RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc* DesiredFormat)
{
    if (VShader >= m_VertexShaders.Size() || !DesiredFormat)
        return 0;
    CKVertexShaderDesc *shader = m_VertexShaders[VShader];
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
    if (PShader >= m_PixelShaders.Size() || !DesiredFormat)
        return 0;
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

void CKDX9RasterizerContext::FlushNonManagedObjects()
{
}

void CKDX9RasterizerContext::ReleaseIndexBuffers()
{
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
	return FALSE;

}

LPDIRECT3DSURFACE9 CKDX9RasterizerContext::GetTempZBuffer(int Width, int Height)
{
	return NULL;
}
