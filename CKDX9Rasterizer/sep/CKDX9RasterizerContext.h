#pragma once
#include "common_include.h"
#include "CKDX9VertexBufferDesc.h"
#include "CKDX9IndexBufferDesc.h"

typedef int TexFromFile;

class CKDX9RasterizerContext : public CKRasterizerContext
{
public:
	//--- Construction/destruction
	CKDX9RasterizerContext(CKDX9RasterizerContext* Driver);
	virtual ~CKDX9RasterizerContext();

	//--- Creation
	virtual BOOL Create(WIN_HANDLE Window, int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, int Bpp = -1, BOOL Fullscreen = 0, int RefreshRate = 0, int Zbpp = -1, int StencilBpp = -1);
	//--- 
	virtual BOOL Resize(int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, CKDWORD Flags = 0);
	virtual BOOL Clear(CKDWORD Flags = CKRST_CTXCLEAR_ALL, CKDWORD Ccol = 0, float Z = 1.0f, CKDWORD Stencil = 0, int RectCount = 0, CKRECT* rects = NULL);
	virtual BOOL BackToFront();

	//--- Scene 
	virtual BOOL BeginScene();
	virtual BOOL EndScene();

	//--- Lighting & Material States
	virtual BOOL SetLight(CKDWORD Light, CKLightData* data);
	virtual BOOL EnableLight(CKDWORD Light, BOOL Enable);
	virtual BOOL SetMaterial(CKMaterialData* mat);

	//--- Viewport State
	virtual BOOL SetViewport(CKViewportData* data);

	//--- Transform Matrix
	virtual BOOL SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix& Mat);

	//--- Render states
	virtual BOOL SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);
	virtual BOOL GetRenderState(VXRENDERSTATETYPE State, CKDWORD* Value);

	//--- Texture States
	virtual BOOL SetTexture(CKDWORD Texture, int Stage = 0);
	virtual BOOL SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value);

	//--- Vertex & Pixel shaders
	virtual BOOL SetVertexShader(CKDWORD VShaderIndex);
	virtual BOOL SetPixelShader(CKDWORD PShaderIndex);
	virtual BOOL SetVertexShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount);
	virtual BOOL SetPixelShaderConstant(CKDWORD Register, const void* Data, CKDWORD CstCount);


	//--- Drawing
	virtual BOOL DrawPrimitive(VXPRIMITIVETYPE pType, WORD* indices, int indexcount, VxDrawPrimitiveData* data);
	virtual BOOL DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex, CKDWORD VertexCount, WORD* indices = NULL, int indexcount = NULL);
	virtual BOOL DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex, CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount);


	//--- Creation of Textures, Sprites and Vertex Buffer
	virtual BOOL CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void* DesiredFormat);

	//--- Vertex Buffers	
	virtual void* LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount, CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
	virtual BOOL UnlockVertexBuffer(CKDWORD VB);

	//--- Textures
	virtual BOOL LoadCubeMapTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, CKRST_CUBEFACE Face, int miplevel = -1);
	virtual BOOL LoadTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, int miplevel = -1);
	virtual	BOOL CopyToTexture(CKDWORD Texture, VxRect* Src, VxRect* Dest, CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS);


	//--- Sprites
	virtual BOOL DrawSprite(CKDWORD Sprite, VxRect* src, VxRect* dst);

	//--- For web content the render context can be transparent (no clear of backbuffer but instead
	//--- a copy of what is currently on screen)
#ifndef _XBOX
	virtual void SetTransparentMode(BOOL Trans);
	virtual void RestoreScreenBackup();
#endif
	virtual	void* GetImplementationSpecificData() { return &m_DirectXData; }

	//--- Utils
	virtual int   CopyToMemoryBuffer(CKRECT* rect, VXBUFFER_TYPE buffer, VxImageDescEx& img_desc);
	virtual int   CopyFromMemoryBuffer(CKRECT* rect, VXBUFFER_TYPE buffer, const VxImageDescEx& img_desc);

	//-- Sets the rendering to occur on a texture (reset the texture format to match )
	virtual BOOL SetTargetTexture(CKDWORD TextureObject, int Width = 0, int Height = 0, CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS, BOOL  GenerateMipMap = FALSE);

	virtual BOOL SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane& PlaneEquation);
	virtual BOOL GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane& PlaneEquation);

	virtual void* LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
	virtual BOOL UnlockIndexBuffer(CKDWORD IB);

	//---------------------------------------------------------------------
	//---- New methods to lock video memory (DX only)
	virtual BOOL LockTextureVideoMemory(CKDWORD Texture, VxImageDescEx& Desc, int MipLevel = 0, VX_LOCKFLAGS Flags = VX_LOCK_DEFAULT);
	virtual BOOL UnlockTextureVideoMemory(CKDWORD Texture, int MipLevel = 0);

	//---- To Enable more direct creation of system objects	without
	//---- CK2_3D holding a copy of the texture
	virtual BOOL CreateTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param);
	virtual BOOL CreateTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param);

	virtual BOOL CreateCubeTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param);
	virtual BOOL CreateCubeTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param);

	virtual BOOL CreateVolumeTextureFromFile(CKDWORD Texture, const char* Filename, TexFromFile* param);
	virtual BOOL CreateVolumeTextureFromFileInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param);
	virtual BOOL LoadVolumeMapTexture(CKDWORD Texture, const VxImageDescEx& SurfDesc, DWORD Depth, int miplevel);

	virtual void EnsureVBBufferNotInUse(CKVertexBufferDesc* desc);
	virtual void EnsureIBBufferNotInUse(CKIndexBufferDesc* desc);
	virtual float GetSurfacesVideoMemoryOccupation(int* NbTextures, int* NbSprites, float* TextureSize, float* SpriteSize);

#ifdef _XBOX
	void SetViewZone(const VxRect& Zone);
	BOOL DrawSolidRect(const VxRect& Zone, DWORD Color);
	BOOL CreateTextureFromXpr(CKDWORD Texture, const char* Filename, TexFromFile* param);
	BOOL CreateTextureFromXprInMemory(CKDWORD Texture, void* mem, DWORD sz, TexFromFile* param);
#endif

protected:
	//-----------------------	
	void	UpdateDirectXData();
	BOOL	InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX9VertexBufferDesc* VB, CKDWORD StartIndex, CKDWORD VertexCount, WORD* indices, int indexcount, BOOL Clip);
	void	SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize);

	//--- Objects creation
	BOOL CreateTexture(CKDWORD Texture, CKTextureDesc* DesiredFormat);
	BOOL CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc* DesiredFormat);
	BOOL CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc* DesiredFormat);
	BOOL CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc* DesiredFormat);
	BOOL CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc* DesiredFormat);


	//---- Cleanup
	void FlushNonManagedObjects();
	void ReleaseIndexBuffers();
	void ClearStreamCache();
	void ReleaseScreenBackup();
	void ResetDevice();
	CKDWORD DX9PresentInterval(DWORD PresentInterval);

#ifdef _NOD3DX
	BOOL LoadSurface(const D3DSURFACE_DESC& ddsd, const D3DLOCKED_RECT& LockRect, const VxImageDescEx& SurfDesc);
#endif
public:
	//--- Temp Z-Buffers for texture rendering...
	void ReleaseTempZBuffers() {
		for (int i = 0; i < 256; ++i) {
			SAFELASTRELEASE(m_TempZBuffers[i]);
		}
	}

	LPDIRECT3DSURFACE9 GetTempZBuffer(int Width, int Height);


	LPDIRECT3DDEVICE9		m_Device;
	D3DPRESENT_PARAMETERS	m_PresentParams;
	VxDirectXData			m_DirectXData;
	BOOL					m_SoftwareVertexProcessing;
	BOOL					m_SoftwareShader;

	//----------------------------------------------------
	//--- Index buffer filled when drawing primitives
	CKDX9IndexBufferDesc* m_IndexBuffer[2]; // Clip/unclipped	

	//-----------------------------------------------------	
	//--- Render Target if rendering is redirected to a texture
	LPDIRECT3DSURFACE9		m_DefaultBackBuffer;	// Backup pointer of previous back buffer
	LPDIRECT3DSURFACE9		m_DefaultDepthBuffer;	// Backup pointer of previous depth buffer

	volatile	BOOL		m_InCreateDestroy;

	//--- For web content the render context can be transparent (no clear of backbuffer but instead
	//--- a copy of what is currently on screen)
	LPDIRECT3DSURFACE9		m_ScreenBackup;

	//-------------------------------------------------
	//--- to avoid redoundant calls to SetVertexShader & SetStreamSource :
	//--- a cache with the current vertex format and source VB
	CKDWORD						m_CurrentVertexShaderCache;
	CKDWORD						m_CurrentVertexFormatCache;
	LPDIRECT3DVERTEXBUFFER9		m_CurrentVertexBufferCache;
	CKDWORD						m_CurrentVertexSizeCache;

	//--------------------------------------------------
	// Render states which must be disabled or which values must be translated...
	CKDWORD* m_TranslatedRenderStates[VXRENDERSTATE_MAXSTATE];

	//-----------------------------------------------------
	// + To do texture rendering, Z-buffers are created when
	// needed for any given size (power of two)
	// These Z buffers are stored in the rasterizer context 
	// TempZbuffers array and are attached when doing 
	// texture rendering 
	LPDIRECT3DSURFACE9	m_TempZBuffers[256];

#ifdef _XBOX
	VxRect					 m_ViewZone;
	CKViewportData			 m_DeviceViewport;

	int m_RealWidth;
	int	m_RealHeight;

	DWORD m_StageTextureInfo;
	VxMatrix m_TextureScaleMatrix[4];
	VxMatrix m_TextureMatrix[4];
#endif

};