#include "CKDX9Rasterizer.h"

CKDX9RasterizerContext::CKDX9RasterizerContext(CKDX9RasterizerDriver* Driver) :
	m_Device(nullptr),
	m_PresentParams(), m_DirectXData(),
	m_SoftwareVertexProcessing(0),
	m_SoftwareShader(0),
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
	m_Driver(Driver),
	m_Owner(static_cast<CKDX9Rasterizer*>(Driver->m_Owner))
{
}

CKDX9RasterizerContext::~CKDX9RasterizerContext()
{
}

BOOL CKDX9RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
	BOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
	m_InCreateDestroy = TRUE;
	if (Window)
	{
		CKRECT Rect;
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
	m_PresentParams.BackBufferFormat = m_Driver->FindNearestRenderTargetFormat(Bpp, !Fullscreen);
	m_PresentParams.AutoDepthStencilFormat = m_Driver->FindNearestDepthFormat(
		m_PresentParams.BackBufferFormat,
		Zbpp, StencilBpp);

}

BOOL CKDX9RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
	return CKRasterizerContext::Resize(PosX, PosY, Width, Height, Flags);
}

BOOL CKDX9RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT* rects)
{
	return CKRasterizerContext::Clear(Flags, Ccol, Z, Stencil, RectCount, rects);
}

BOOL CKDX9RasterizerContext::BackToFront(CKBOOL vsync)
{
	return CKRasterizerContext::BackToFront(vsync);
}

BOOL CKDX9RasterizerContext::BeginScene()
{
	return CKRasterizerContext::BeginScene();
}

BOOL CKDX9RasterizerContext::EndScene()
{
	return CKRasterizerContext::EndScene();
}

BOOL CKDX9RasterizerContext::SetLight(CKDWORD Light, CKLightData* data)
{
	return CKRasterizerContext::SetLight(Light, data);
}

BOOL CKDX9RasterizerContext::EnableLight(CKDWORD Light, BOOL Enable)
{
	return CKRasterizerContext::EnableLight(Light, Enable);
}

BOOL CKDX9RasterizerContext::SetMaterial(CKMaterialData* mat)
{
	return CKRasterizerContext::SetMaterial(mat);
}

BOOL CKDX9RasterizerContext::SetViewport(CKViewportData* data)
{
	return CKRasterizerContext::SetViewport(data);
}

BOOL CKDX9RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix& Mat)
{
	return CKRasterizerContext::SetTransformMatrix(Type, Mat);
}

BOOL CKDX9RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
	return CKRasterizerContext::SetRenderState(State, Value);
}

BOOL CKDX9RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD* Value)
{
	return CKRasterizerContext::GetRenderState(State, Value);
}

BOOL CKDX9RasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
	return CKRasterizerContext::SetTexture(Texture, Stage);
}

BOOL CKDX9RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
	return CKRasterizerContext::SetTextureStageState(Stage, Tss, Value);
}

BOOL CKDX9RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
	return CKRasterizerContext::SetVertexShader(VShaderIndex);
}

BOOL CKDX9RasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
	return CKRasterizerContext::SetPixelShader(PShaderIndex);
}

BOOL CKDX9RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount)
{
	return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}

BOOL CKDX9RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount)
{
	return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}

BOOL CKDX9RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, WORD* indices, int indexcount,
	VxDrawPrimitiveData* data)
{
	return CKRasterizerContext::DrawPrimitive(pType, indices, indexcount, data);
}

BOOL CKDX9RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
	CKDWORD VertexCount, WORD* indices, int indexcount)
{
	return CKRasterizerContext::DrawPrimitiveVB(pType, VertexBuffer, StartIndex, VertexCount, indices, indexcount);
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

BOOL CKDX9RasterizerContext::LoadCubeMapTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, CKRST_CUBEFACE Face,
	int miplevel)
{
	return CKRasterizerContext::LoadCubeMapTexture(Texture, SurfDesc, Face, miplevel);
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

BOOL CKDX9RasterizerContext::CreateTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateCubeTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateCubeTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateVolumeTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateVolumeTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz,
	TexFromFile* param)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::LoadVolumeMapTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, DWORD Depth,
	int miplevel)
{
	return FALSE;

}

void CKDX9RasterizerContext::EnsureVBBufferNotInUse(CKVertexBufferDesc* desc)
{
}

void CKDX9RasterizerContext::EnsureIBBufferNotInUse(CKIndexBufferDesc* desc)
{
}

float CKDX9RasterizerContext::GetSurfacesVideoMemoryOccupation(int* NbTextures, int* NbSprites, float* TextureSize,
	float* SpriteSize)
{
	return 0.0;
}

BOOL CKDX9RasterizerContext::FlushPendingGPUCommands()
{
	return FALSE;

}

void CKDX9RasterizerContext::UpdateDirectXData()
{
}

BOOL CKDX9RasterizerContext::InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc* VB,
	CKDWORD StartIndex, CKDWORD VertexCount, WORD* indices, int indexcount, BOOL Clip)
{
	return FALSE;

}

void CKDX9RasterizerContext::SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize)
{
}

BOOL CKDX9RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc* DesiredFormat)
{
	return FALSE;

}

BOOL CKDX9RasterizerContext::TextureCanUseAutoGenMipMap(D3DFORMAT TextureFormat)
{
	return FALSE;

}

void CKDX9RasterizerContext::UpdateTextureBPL(CKDWORD Texture)
{
}

void CKDX9RasterizerContext::FlushNonManagedObjects()
{
}

void CKDX9RasterizerContext::ReleaseIndexBuffers()
{
}

void CKDX9RasterizerContext::ClearStreamCache()
{
}

void CKDX9RasterizerContext::ReleaseScreenBackup()
{
}

void CKDX9RasterizerContext::ResetDevice()
{
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
