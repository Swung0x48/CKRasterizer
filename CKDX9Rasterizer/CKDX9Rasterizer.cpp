#include "CKDX9Rasterizer.h"

CKRasterizer* CKDX9RasterizerStart(WIN_HANDLE AppWnd) {
	HMODULE handle = LoadLibraryA("d3d9.dll");
	if (handle) {
		CKRasterizer* rasterizer = new CKDX9Rasterizer;
		if (!rasterizer)
			return NULL;
		if (!rasterizer->Start(AppWnd)) {
			delete rasterizer;
			FreeLibrary(handle);
			return nullptr;
		}
		return rasterizer;
	}
	return nullptr;
}

void CKDX9RasterizerClose(CKRasterizer* rst)
{
	if (rst)
	{
		rst->Close();
		delete rst;
	}
}

PLUGIN_EXPORT void CKRasterizerGetInfo(CKRasterizerInfo* info)
{
	info->StartFct = CKDX9RasterizerStart;
	info->CloseFct = CKDX9RasterizerClose;
	info->Desc = "DirectX 9 Rasterizer";
}

CKDX9Rasterizer::CKDX9Rasterizer(void): m_D3D9(NULL), m_Init(FALSE) {

}

CKDX9Rasterizer::~CKDX9Rasterizer(void)
{
}

XBOOL CKDX9Rasterizer::Start(WIN_HANDLE AppWnd)
{
	InitBlendStages();
	this->m_MainWindow = AppWnd;
	this->m_Init = TRUE;
	
	HRESULT result = E_FAIL;

	// Create the D3D object, which is needed to create the D3DDevice.
	if (FAILED(result = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_D3D9)))
	{
		m_D3D9 = NULL;
		return FALSE;
	}
    if (m_D3D9)
    {
        UINT count = m_D3D9->GetAdapterCount();
		for (UINT i = 0; i < count; ++i) {
			CKDX9RasterizerDriver* driver = new CKDX9RasterizerDriver(this);
			if (!driver->InitializeCaps(i, D3DDEVTYPE_HAL))
				delete driver;
			m_Drivers.PushBack(driver);
		}
	}

	return TRUE;
}

void CKDX9Rasterizer::Close(void)
{
	if (!m_Init)
		return;
	if (m_D3D9)
		m_D3D9->Release();
	while (m_Drivers.Size() != 0) {
		CKRasterizerDriver* driver = m_Drivers.PopBack();
		delete driver;
	}
	m_D3D9 = NULL;
	m_Init = FALSE;
}

void CKDX9Rasterizer::InitBlendStages()
{
	memset(m_BlendStages, NULL, sizeof(m_BlendStages));

	CKStageBlend* b = new CKStageBlend;
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
