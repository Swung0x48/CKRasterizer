#include "CKDX9Rasterizer.h"

#include "CKD3DX9.h"

PFN_D3DXDeclaratorFromFVF D3DXDeclaratorFromFVF = NULL;
PFN_D3DXFVFFromDeclarator D3DXFVFFromDeclarator = NULL;
PFN_D3DXAssembleShader D3DXAssembleShader = NULL;
PFN_D3DXDisassembleShader D3DXDisassembleShader = NULL;
PFN_D3DXLoadSurfaceFromSurface D3DXLoadSurfaceFromSurface = NULL;
PFN_D3DXLoadSurfaceFromMemory D3DXLoadSurfaceFromMemory = NULL;
PFN_D3DXCreateTextureFromFileExA D3DXCreateTextureFromFileExA = NULL;

CKRasterizer *CKDX9RasterizerStart(WIN_HANDLE AppWnd)
{
    HMODULE handle = LoadLibraryA("d3d9.dll");
    if (handle)
    {
        CKRasterizer *rasterizer = new CKDX9Rasterizer;
        if (!rasterizer)
            return NULL;
        if (!rasterizer->Start(AppWnd))
        {
            delete rasterizer;
            FreeLibrary(handle);
            return NULL;
        }
        return rasterizer;
    }
    return NULL;
}

void CKDX9RasterizerClose(CKRasterizer *rst)
{
    if (rst)
    {
        rst->Close();
        delete rst;
    }
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo *info)
{
    info->StartFct = CKDX9RasterizerStart;
    info->CloseFct = CKDX9RasterizerClose;
    info->Desc = "DirectX 9 Rasterizer";
}

CKDX9Rasterizer::CKDX9Rasterizer() : m_D3D9(NULL), m_Init(FALSE), m_BlendStages() {}

CKDX9Rasterizer::~CKDX9Rasterizer() {}

XBOOL CKDX9Rasterizer::Start(WIN_HANDLE AppWnd)
{
    InitBlendStages();
    this->m_MainWindow = AppWnd;
    this->m_Init = TRUE;

    HRESULT hr = E_FAIL;

    // Create the D3D object, which is needed to create the D3DDevice.
    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_D3D9);
    if (FAILED(hr))
    {
        m_D3D9 = NULL;
        return FALSE;
    }

	// Load D3DX
	if (!D3DXDeclaratorFromFVF ||
        !D3DXFVFFromDeclarator ||
        !D3DXAssembleShader ||
        !D3DXDisassembleShader ||
        !D3DXLoadSurfaceFromSurface ||
        !D3DXLoadSurfaceFromMemory ||
        !D3DXCreateTextureFromFileExA)
	{
		HMODULE module = LoadLibrary(TEXT("d3dx9_43.dll"));
		if (module)
		{
            D3DXDeclaratorFromFVF = reinterpret_cast<PFN_D3DXDeclaratorFromFVF>(GetProcAddress(module, "D3DXDeclaratorFromFVF"));
            D3DXFVFFromDeclarator = reinterpret_cast<PFN_D3DXFVFFromDeclarator>(GetProcAddress(module, "D3DXFVFFromDeclarator"));
			D3DXAssembleShader = reinterpret_cast<PFN_D3DXAssembleShader>(GetProcAddress(module, "D3DXAssembleShader"));
			D3DXDisassembleShader = reinterpret_cast<PFN_D3DXDisassembleShader>(GetProcAddress(module, "D3DXDisassembleShader"));
			D3DXLoadSurfaceFromSurface = reinterpret_cast<PFN_D3DXLoadSurfaceFromSurface>(GetProcAddress(module, "D3DXLoadSurfaceFromSurface"));
            D3DXLoadSurfaceFromMemory = reinterpret_cast<PFN_D3DXLoadSurfaceFromMemory>(GetProcAddress(module, "D3DXLoadSurfaceFromMemory"));
            D3DXCreateTextureFromFileExA = reinterpret_cast<PFN_D3DXCreateTextureFromFileExA>(GetProcAddress(module, "D3DXCreateTextureFromFileExA"));
		}
	}

    UINT count = m_D3D9->GetAdapterCount();
    for (UINT i = 0; i < count; ++i)
    {
        CKDX9RasterizerDriver *driver = new CKDX9RasterizerDriver(this);
        if (!driver->InitializeCaps(i, D3DDEVTYPE_HAL))
            delete driver;
        m_Drivers.PushBack(driver);
    }

    return TRUE;
}

void CKDX9Rasterizer::Close()
{
    if (!m_Init)
        return;
    if (m_D3D9)
        m_D3D9->Release();
    while (m_Drivers.Size() != 0)
    {
        CKRasterizerDriver *driver = m_Drivers.PopBack();
        delete driver;
    }
    m_D3D9 = NULL;
    m_Init = FALSE;
}

void CKDX9Rasterizer::InitBlendStages()
{
    memset(m_BlendStages, NULL, sizeof(m_BlendStages));

    CKStageBlend *b = new CKStageBlend;
    b->Cop = D3DTOP_MODULATE;
    b->Carg1 = D3DTA_TEXTURE;
    b->Carg2 = D3DTA_CURRENT;
    b->Aop = D3DTOP_SELECTARG1;
    b->Aarg1 = D3DTA_CURRENT;
    b->Aarg2 = D3DTA_CURRENT;
    m_BlendStages[STAGEBLEND(VXBLEND_ZERO, VXBLEND_SRCCOLOR)] = b;

    b = new CKStageBlend;
    b->Cop = D3DTOP_MODULATE;
    b->Carg1 = D3DTA_TEXTURE;
    b->Carg2 = D3DTA_CURRENT;
    b->Aop = D3DTOP_SELECTARG1;
    b->Aarg1 = D3DTA_CURRENT;
    b->Aarg2 = D3DTA_CURRENT;
    m_BlendStages[STAGEBLEND(VXBLEND_DESTCOLOR, VXBLEND_ZERO)] = b;

    b = new CKStageBlend;
    b->Cop = D3DTOP_ADD;
    b->Carg1 = D3DTA_TEXTURE;
    b->Carg2 = D3DTA_CURRENT;
    b->Aop = D3DTOP_SELECTARG1;
    b->Aarg1 = D3DTA_CURRENT;
    b->Aarg2 = D3DTA_CURRENT;
    m_BlendStages[STAGEBLEND(VXBLEND_ONE, VXBLEND_ONE)] = b;
}

CKBOOL CKDX9VertexShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKVertexShaderDesc *Format)
{
    if (Format != this)
    {
        Owner = Ctx;
        m_Function = Format->m_Function;
        m_FunctionSize = Format->m_FunctionSize;
    }

    SAFERELEASE(DxShader);
    return SUCCEEDED(Ctx->m_Device->CreateVertexShader(m_Function, &DxShader));
}

CKDX9VertexShaderDesc::~CKDX9VertexShaderDesc()
{
    SAFERELEASE(DxShader);
}

CKBOOL CKDX9PixelShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKPixelShaderDesc *Format)
{
    if (Format != this)
    {
        Owner = Ctx;
        m_Function = Format->m_Function;
        m_FunctionSize = Format->m_FunctionSize;
    }

    SAFERELEASE(DxShader);
    return SUCCEEDED(Ctx->m_Device->CreatePixelShader(m_Function, &DxShader));
}

CKDX9PixelShaderDesc::~CKDX9PixelShaderDesc()
{
    SAFERELEASE(DxShader);
}