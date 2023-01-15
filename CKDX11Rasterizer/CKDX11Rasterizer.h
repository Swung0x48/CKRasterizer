#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "CKRasterizer.h"
#include <Windows.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl.h>

#ifdef _DEBUG
    #include <dxgidebug.h>
#endif

class CKDX11Rasterizer : public CKRasterizer
{
public:
	CKDX11Rasterizer(void);
	virtual ~CKDX11Rasterizer(void);

	virtual XBOOL Start(WIN_HANDLE AppWnd);
	virtual void Close(void);

public:
    Microsoft::WRL::ComPtr<IDXGIFactory1> m_Factory;
};

class CKDX11RasterizerDriver : public CKRasterizerDriver
{
public:
    CKDX11RasterizerDriver(CKDX11Rasterizer *rst);
    virtual ~CKDX11RasterizerDriver();

    //--- Contexts
    virtual CKRasterizerContext *CreateContext();
    
    CKBOOL InitializeCaps(Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter, Microsoft::WRL::ComPtr<IDXGIOutput> Output);

public:
    CKBOOL m_Inited;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_Adapter;
    Microsoft::WRL::ComPtr<IDXGIOutput> m_Output;
    DXGI_ADAPTER_DESC1 m_AdapterDesc;
    DXGI_OUTPUT_DESC m_OutputDesc;
};

class CKDX11RasterizerContext : public CKRasterizerContext
{
public:
    //--- Construction/destruction
    CKDX11RasterizerContext();
    virtual ~CKDX11RasterizerContext();

    //--- Creation
    virtual CKBOOL Create(WIN_HANDLE Window, int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, int Bpp = -1,
                          CKBOOL Fullscreen = 0, int RefreshRate = 0, int Zbpp = -1, int StencilBpp = -1);
    //---
    virtual CKBOOL Resize(int PosX = 0, int PosY = 0, int Width = 0, int Height = 0, CKDWORD Flags = 0);
    virtual CKBOOL Clear(CKDWORD Flags = CKRST_CTXCLEAR_ALL, CKDWORD Ccol = 0, float Z = 1.0f, CKDWORD Stencil = 0,
                         int RectCount = 0, CKRECT *rects = NULL);
    virtual CKBOOL BackToFront(CKBOOL vsync);

    //--- Scene
    virtual CKBOOL BeginScene();
    virtual CKBOOL EndScene();

    //--- Lighting & Material States
    virtual CKBOOL SetLight(CKDWORD Light, CKLightData *data);
    virtual CKBOOL EnableLight(CKDWORD Light, CKBOOL Enable);
    virtual CKBOOL SetMaterial(CKMaterialData *mat);

    //--- Viewport State
    virtual CKBOOL SetViewport(CKViewportData *data);

    //--- Transform Matrix
    virtual CKBOOL SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat);

    //--- Render states
    virtual CKBOOL SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);
    virtual CKBOOL GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value);

    //--- Texture States
    virtual CKBOOL SetTexture(CKDWORD Texture, int Stage = 0);
    virtual CKBOOL SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value);

    //--- Vertex & Pixel shaders
    virtual CKBOOL SetVertexShader(CKDWORD VShaderIndex);
    virtual CKBOOL SetPixelShader(CKDWORD PShaderIndex);
    virtual CKBOOL SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount);
    virtual CKBOOL SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount);

    //--- Drawing
    virtual CKBOOL DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount, VxDrawPrimitiveData *data);
    virtual CKBOOL DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex, CKDWORD VertexCount,
                                   CKWORD *indices = NULL, int indexcount = NULL);
    virtual CKBOOL DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                     CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount);

    //--- Creation of Textures, Sprites and Vertex Buffer
    CKBOOL CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat) override;

    //--- Vertex Buffers
    virtual void *LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
                                   CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
    virtual CKBOOL UnlockVertexBuffer(CKDWORD VB);

    //--- Textures
    virtual CKBOOL LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel = -1);
    virtual CKBOOL CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS);
    //-- Sets the rendering to occur on a texture (reset the texture format to match )
    virtual CKBOOL SetTargetTexture(CKDWORD TextureObject, int Width = 0, int Height = 0,
                                    CKRST_CUBEFACE Face = CKRST_CUBEFACE_XPOS, CKBOOL GenerateMipMap = FALSE);

    //--- Sprites
    virtual CKBOOL DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst);

    //--- Utils
    virtual int CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc);
    virtual int CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc);

//    virtual void *GetImplementationSpecificData() { return &m_DirectXData; }

    virtual CKBOOL SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation);
    virtual CKBOOL GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation);

    virtual void *LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount,
                                  CKRST_LOCKFLAGS Lock = CKRST_LOCK_DEFAULT);
    virtual CKBOOL UnlockIndexBuffer(CKDWORD IB);

    //---------------------------------------------------------------------
    //---- New methods to lock video memory (DX only)
    //virtual CKBOOL LockTextureVideoMemory(CKDWORD Texture, VxImageDescEx &Desc, int MipLevel = 0,
    //                                      VX_LOCKFLAGS Flags = VX_LOCK_DEFAULT);
    //virtual CKBOOL UnlockTextureVideoMemory(CKDWORD Texture, int MipLevel = 0);

    //---- To Enable more direct creation of system objects	without
    //---- CK2_3D holding a copy of the texture
//    virtual CKBOOL CreateTextureFromFile(CKDWORD Texture, const char *Filename, TexFromFile *param);

protected:
//    CKBOOL InternalDrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDX11VertexBufferDesc *VB, CKDWORD StartIndex,
//                                   CKDWORD VertexCount, CKWORD *indices, int indexcount, CKBOOL Clip);
//    void SetupStreams(LPDIRECT3DVERTEXBUFFER9 Buffer, CKDWORD VFormat, CKDWORD VSize);

    //--- Objects creation
    CKBOOL CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat);
    CKBOOL CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat);
    CKBOOL CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat);
    CKBOOL CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat);
    CKBOOL CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat);
#ifdef _NOD3DX
    CKBOOL LoadSurface(const D3DSURFACE_DESC &ddsd, const D3DLOCKED_RECT &LockRect, const VxImageDescEx &SurfDesc);
#endif

    //--- Temp Z-Buffers for texture rendering...
//    void ReleaseTempZBuffers()
//    {
//        for (int i = 0; i < 256; ++i)
//        {
//            SAFELASTRELEASE(m_TempZBuffers[i]);
//        }
//    }

//    LPDIRECT3DSURFACE9 GetTempZBuffer(int Width, int Height);

public:
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_Swapchain;
    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_BackBuffer;

    D3D_FEATURE_LEVEL m_FeatureLevel;
    const float m_ClearColor[4] = {
        0.0f,
        0.2f,
        0.4f,
        1.0f,
    };
        //    IDirect3DDevice9Ex *m_Device;
//    D3DPRESENT_PARAMETERS m_PresentParams;
//    VxDirectXData m_DirectXData;
//    CKBOOL m_SoftwareVertexProcessing;
//
//    //----------------------------------------------------
//    //--- Index buffer filled when drawing primitives
//    CKDX11IndexBufferDesc *m_IndexBuffer[2]; // Clip/unclipped
//
//    int m_CurrentTextureIndex;
//
//    //-----------------------------------------------------
//    //--- Render Target if rendering is redirected to a texture
//    LPDIRECT3DSURFACE9 m_DefaultBackBuffer; // Backup pointer of previous back buffer
//    LPDIRECT3DSURFACE9 m_DefaultDepthBuffer; // Backup pointer of previous depth buffer
//
//    volatile CKBOOL m_InCreateDestroy;
//
//    //--- For web content the render context can be transparent (no clear of backbuffer but instead
//    //--- a copy of what is currently on screen)
//    LPDIRECT3DSURFACE9 m_ScreenBackup;
//
//    //-------------------------------------------------
//    //--- to avoid redoundant calls to SetVertexShader & SetStreamSource :
//    //--- a cache with the current vertex format and source VB
//    CKDWORD m_CurrentVertexShaderCache;
//    CKDWORD m_CurrentVertexFormatCache;
//    LPDIRECT3DVERTEXBUFFER9 m_CurrentVertexBufferCache;
//    CKDWORD m_CurrentVertexSizeCache;
//
//    XBitArray m_StateCacheHitMask;
//    XBitArray m_StateCacheMissMask;
//
//    //--------------------------------------------------
//    // Render states which must be disabled or which values must be translated...
//    CKDWORD *m_TranslatedRenderStates[VXRENDERSTATE_MAXSTATE];
//
//    LPDIRECT3DSTATEBLOCK9 m_TextureMinFilterStateBlocks[8][8];
//    LPDIRECT3DSTATEBLOCK9 m_TextureMagFilterStateBlocks[8][8];
//    LPDIRECT3DSTATEBLOCK9 m_TextureMapBlendStateBlocks[10][8];
//
//    //-----------------------------------------------------
//    // + To do texture rendering, Z-buffers are created when
//    // needed for any given size (power of two)
//    // These Z buffers are stored in the rasterizer context
//    // TempZbuffers array and are attached when doing
//    // texture rendering
//    LPDIRECT3DSURFACE9 m_TempZBuffers[NBTEMPZBUFFER];

    CKDX11Rasterizer *m_Owner;
};
