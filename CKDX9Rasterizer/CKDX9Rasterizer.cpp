#include "CKDX9Rasterizer.h"

CKDX9Rasterizer::~CKDX9Rasterizer(void)
{
}

XBOOL CKDX9Rasterizer::Start(WIN_HANDLE AppWnd)
{
	InitBlendStages();
	this->m_MainWindow = AppWnd;
	this->m_Init = TRUE;

	IDirect3D9Ex* pD3D = NULL;
	IDirect3DDevice9Ex* pDevice = NULL;
	HRESULT result = E_FAIL;

	// Create the D3D object, which is needed to create the D3DDevice.
	if (FAILED(result = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D)))
	{
		m_D3D9 = NULL;
		return result;
	}
	m_D3D9 = pD3D;
	if (pD3D) {
		UINT count = pD3D->GetAdapterCount();
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
}

void CKDX9Rasterizer::InitBlendStages()
{
	// TODO: Check index
	memset(this->m_BlendStages, 0, sizeof(this->m_BlendStages));
	CKStageBlend* b = new CKStageBlend;
	b->Cop = D3DTOP_MODULATE;
	b->Carg1 = 2;
	b->Carg2 = 1;
	b->Aop = D3DTOP_SELECTARG1;
	b->Aarg1 = 1;
	b->Aarg2 = 1;
	this->m_BlendStages[0] = b; // 19?
	b = new CKStageBlend;
	b->Cop = D3DTOP_MODULATE;
	b->Carg1 = 2;
	b->Carg2 = 1;
	b->Aop = D3DTOP_SELECTARG1;
	b->Aarg1 = 1;
	b->Aarg2 = 1;
	this->m_BlendStages[2] = b; // 145?
	b = new CKStageBlend;
	b->Cop = D3DTOP_ADD;
	b->Carg1 = 2;
	b->Carg2 = 1;
	b->Aop = D3DTOP_SELECTARG1;
	b->Aarg1 = 1;
	b->Aarg2 = 1;
	this->m_BlendStages[1] = b; // 34?
}


