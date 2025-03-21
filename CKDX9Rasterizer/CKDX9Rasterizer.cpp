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

CKDX9Rasterizer::~CKDX9Rasterizer()
{
    Close();
}

XBOOL CKDX9Rasterizer::Start(WIN_HANDLE AppWnd)
{
    InitBlendStages();
    m_MainWindow = AppWnd;
    m_Init = TRUE;

    // Create the D3D object, which is needed to create the D3DDevice.
#ifdef USE_D3D9EX
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_D3D9);
    if (FAILED(hr))
    {
        m_D3D9 = NULL;
        return FALSE;
    }
#else
    m_D3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_D3D9)
        return FALSE;
#endif

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
        else
            m_Drivers.PushBack(driver);
    }

    return TRUE;
}

void CKDX9Rasterizer::Close()
{
    if (!m_Init)
        return;

    // Clean up blend stages
    for (int i = 0; i < 256; i++)
    {
        if (m_BlendStages[i])
        {
            delete m_BlendStages[i];
            m_BlendStages[i] = NULL;
        }
    }

    if (m_D3D9)
    {
        m_D3D9->Release();
        m_D3D9 = NULL;
    }

    while (m_Drivers.Size() != 0)
    {
        CKRasterizerDriver *driver = m_Drivers.PopBack();
        delete driver;
    }

    m_Init = FALSE;
}

void CKDX9Rasterizer::InitBlendStages()
{
    memset(m_BlendStages, NULL, sizeof(m_BlendStages));

    // Modulate (ZERO, SRCCOLOR and DESTCOLOR, ZERO)
    CreateBlendStage(VXBLEND_ZERO, VXBLEND_SRCCOLOR, 
                     D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT);
            
    CreateBlendStage(VXBLEND_DESTCOLOR, VXBLEND_ZERO, 
                     D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT);

    // Additive (ONE, ONE)
    CreateBlendStage(VXBLEND_ONE, VXBLEND_ONE, 
                     D3DTOP_ADD, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT);

    // Replace (ONE, ZERO)
    CreateBlendStage(VXBLEND_ONE, VXBLEND_ZERO, 
                     D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Alpha blend (SRCALPHA, INVSRCALPHA)
    CreateBlendStage(VXBLEND_SRCALPHA, VXBLEND_INVSRCALPHA, 
                     D3DTOP_BLENDTEXTUREALPHA, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Premultiplied alpha (ONE, INVSRCALPHA)
    CreateBlendStage(VXBLEND_ONE, VXBLEND_INVSRCALPHA, 
                     D3DTOP_BLENDTEXTUREALPHA, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Multiply (ZERO, INVSRCCOLOR)
    CreateBlendStage(VXBLEND_ZERO, VXBLEND_INVSRCCOLOR, 
                     D3DTOP_MODULATEINVALPHA_ADDCOLOR, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT);

    // Additive alpha (SRCALPHA, ONE)
    CreateBlendStage(VXBLEND_SRCALPHA, VXBLEND_ONE, 
                     D3DTOP_BLENDTEXTUREALPHA, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Modulate 2X (double brightness)
    CreateBlendStage(VXBLEND_DESTCOLOR, VXBLEND_SRCCOLOR,
                     D3DTOP_MODULATE2X, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_MODULATE2X, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Modulate 4X (quadruple brightness)
    CreateBlendStage(VXBLEND_DESTCOLOR, VXBLEND_SRCALPHA,
                     D3DTOP_MODULATE4X, D3DTA_TEXTURE, D3DTA_CURRENT,
                     D3DTOP_MODULATE4X, D3DTA_TEXTURE, D3DTA_CURRENT);

    // Subtract
    CreateBlendStage(VXBLEND_INVSRCCOLOR, VXBLEND_SRCCOLOR,
                     D3DTOP_SUBTRACT, D3DTA_CURRENT, D3DTA_TEXTURE,
                     D3DTOP_SUBTRACT, D3DTA_CURRENT, D3DTA_TEXTURE);
}

void CKDX9Rasterizer::CreateBlendStage(VXBLEND_MODE srcBlend, VXBLEND_MODE destBlend,
                                       D3DTEXTUREOP colorOp, DWORD colorArg1, DWORD colorArg2,
                                       D3DTEXTUREOP alphaOp, DWORD alphaArg1, DWORD alphaArg2)
{
    CKStageBlend *b = new CKStageBlend;
    b->Cop = colorOp;
    b->Carg1 = colorArg1;
    b->Carg2 = colorArg2;
    b->Aop = alphaOp;
    b->Aarg1 = alphaArg1;
    b->Aarg2 = alphaArg2;
    m_BlendStages[STAGEBLEND(srcBlend, destBlend)] = b;
}