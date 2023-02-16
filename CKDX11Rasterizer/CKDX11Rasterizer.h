#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#define DYNAMIC_VBO_COUNT 64
#define DYNAMIC_IBO_COUNT 64

#include "CKRasterizer.h"
#include <Windows.h>
#include <d3d11.h>
#include <d3d10.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <dxgi1_5.h>

#ifdef _DEBUG
    #include <dxgidebug.h>
#endif

#include <string>
#include <unordered_map>
#include <XBitArray.h>

#include "FlexibleVertexFormat.h"

#include "CKDX11TextureFilter.h"
using Microsoft::WRL::ComPtr;

class CKDX11Rasterizer : public CKRasterizer
{
public:
	CKDX11Rasterizer(void);
	virtual ~CKDX11Rasterizer(void);

	virtual XBOOL Start(WIN_HANDLE AppWnd);
	virtual void Close(void);

public:
    ComPtr<IDXGIFactory1> m_Factory;
    CKBOOL m_TearingSupport = FALSE;
    std::string m_DXGIVersionString = "1.1";
};

class CKDX11RasterizerDriver : public CKRasterizerDriver
{
public:
    CKDX11RasterizerDriver(CKDX11Rasterizer *rst);
    virtual ~CKDX11RasterizerDriver();

    //--- Contexts
    virtual CKRasterizerContext *CreateContext();
    
    CKBOOL InitializeCaps(ComPtr<IDXGIAdapter1> Adapter, ComPtr<IDXGIOutput> Output);

public:
    CKBOOL m_Inited;
    ComPtr<IDXGIAdapter1> m_Adapter;
    ComPtr<IDXGIOutput> m_Output;
    DXGI_ADAPTER_DESC1 m_AdapterDesc;
    DXGI_OUTPUT_DESC m_OutputDesc;
};

class CKDX11RasterizerContext;

typedef struct CKDX11VertexBufferDesc : public CKVertexBufferDesc
{
public:
    ComPtr<ID3D11Buffer> DxBuffer;
    D3D11_BUFFER_DESC DxDesc;
    CKDX11VertexBufferDesc() { ZeroMemory(&DxDesc, sizeof(D3D11_BUFFER_DESC)); }
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
    virtual void *Lock(CKDX11RasterizerContext *ctx, CKDWORD offset, CKDWORD len, bool overwrite);
    virtual void Unlock(CKDX11RasterizerContext *ctx);
    virtual void Bind(CKDX11RasterizerContext *ctx);
} CKDX11VertexBufferDesc;

typedef struct CKDX11IndexBufferDesc : public CKIndexBufferDesc
{
public:
    ComPtr<ID3D11Buffer> DxBuffer;
    D3D11_BUFFER_DESC DxDesc;
    CKDX11IndexBufferDesc() { ZeroMemory(&DxDesc, sizeof(D3D11_BUFFER_DESC)); }
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
    virtual void *Lock(CKDX11RasterizerContext *ctx, CKDWORD offset, CKDWORD len, bool overwrite);
    virtual void Unlock(CKDX11RasterizerContext *ctx);
    virtual void Bind(CKDX11RasterizerContext *ctx);
} CKDX11IndexBufferDesc;

typedef struct CKDX11VertexShaderDesc : public CKVertexShaderDesc
{
    ComPtr<ID3DBlob> DxBlob;
    ComPtr<ID3D11VertexShader> DxShader;
    LPCSTR DxEntryPoint = "VShader";
    LPCSTR DxTarget = "vs_4_0";
    ComPtr<ID3DBlob> DxErrorMsgs;
    CKDWORD DxFVF;
    std::vector<D3D11_INPUT_ELEMENT_DESC> DxInputElementDesc;
    ComPtr<ID3D11InputLayout> DxInputLayout;
    virtual CKBOOL Compile(CKDX11RasterizerContext *ctx);
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
    virtual void Bind(CKDX11RasterizerContext *ctx);
} CKDX11VertexShaderDesc;

typedef struct CKDX11PixelShaderDesc : public CKPixelShaderDesc
{
    ComPtr<ID3DBlob> DxBlob;
    ComPtr<ID3D11PixelShader> DxShader;
    LPCSTR DxEntryPoint = "PShader";
    LPCSTR DxTarget = "ps_4_0";
    ComPtr<ID3DBlob> DxErrorMsgs;
    virtual CKBOOL Compile(CKDX11RasterizerContext *ctx);
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
    virtual void Bind(CKDX11RasterizerContext *ctx);
} CKDX11PixelShaderDesc;

static constexpr uint32_t AFLG_ALPHATESTEN = 0x10U;
typedef struct ConstantBufferStruct
{
    VxMatrix TotalMatrix;
    VxMatrix ViewportMatrix;
    uint32_t AlphaFlags = 0;
    float AlphaThreshold = 0.0f;
    uint64_t _padding;
} ConstantBufferStruct;


typedef struct CKDX11ConstantBufferDesc
{
public:
    ComPtr<ID3D11Buffer> DxBuffer;
    D3D11_BUFFER_DESC DxDesc;
    CKDX11ConstantBufferDesc() { ZeroMemory(&DxDesc, sizeof(D3D11_BUFFER_DESC)); }
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
} CKDX11ConstantBufferDesc;

typedef struct CKDX11TextureDesc : CKTextureDesc
{
    D3D11_TEXTURE2D_DESC DxDesc;
    ComPtr<ID3D11Texture2D> DxTexture;
    ComPtr<ID3D11ShaderResourceView> DxSRV;
    CKDX11TextureDesc(CKTextureDesc *desc);
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx, void *data);
    void Bind(CKDX11RasterizerContext *ctx);
    void Load(CKDX11RasterizerContext *ctx, void *data);
} CKDX11TextureDesc;

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
    virtual CKBOOL DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD StartVIndex, CKDWORD VertexCount,
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

    virtual void *GetImplementationSpecificData() { return &m_DirectXData; }

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
    //--- Objects creation
    CKBOOL CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat);
    CKBOOL CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat);
    CKBOOL CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat);
    CKBOOL CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat);
    CKBOOL CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat);
    CKBOOL AssemblyInput(CKDX11VertexBufferDesc *vbo, CKDX11IndexBufferDesc *ibo, VXPRIMITIVETYPE pType);
    CKBOOL InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKDX11VertexBufferDesc *vbo,
                                                          CKDWORD StartVertex, CKDWORD VertexCount, CKWORD *indices,
                                                          int indexcount);
    CKBOOL InternalSetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);

    CKDX11IndexBufferDesc* GenerateIB(void *indices, int indexcount, int *startIndex);

    CKDX11IndexBufferDesc *TriangleFanToList(CKWORD VOffset, CKDWORD VCount, int *startIndex, int* newIndexCount);
    CKDX11IndexBufferDesc *TriangleFanToList(CKWORD *indices, int count, int *startIndex, int *newIndexCount);
    void SetTitleStatus(const char *fmt, ...);
#ifdef _NOD3DX
    CKBOOL LoadSurface(const D3DSURFACE_DESC &ddsd, const D3DLOCKED_RECT &LockRect, const VxImageDescEx &SurfDesc);
#endif
    
public:
    ComPtr<IDXGISwapChain> m_Swapchain;
    ComPtr<ID3D11Device> m_Device;
    ComPtr<ID3D11DeviceContext> m_DeviceContext;
    ComPtr<ID3D11RenderTargetView> m_BackBuffer;
    ComPtr<ID3D11DepthStencilView> m_DepthStencilView;

    D3D11_SAMPLER_DESC m_SamplerDesc[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ComPtr<ID3D11SamplerState> m_SamplerState[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    CKBOOL m_SamplerStateUpToDate[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {TRUE};
    CKDX11TextureFilter m_Filter[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    ID3D11SamplerState *m_SamplerRaw[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {nullptr};

    D3D11_DEPTH_STENCIL_DESC m_DepthStencilDesc;
    ComPtr<ID3D11DepthStencilState> m_DepthStencilState;
    CKBOOL m_DepthStencilStateUpToDate = TRUE;

    ComPtr<ID3D11BlendState> m_BlendState;
    D3D11_BLEND_DESC m_BlendStateDesc;
    CKBOOL m_BlendStateUpToDate = TRUE;

    ComPtr<ID3D11RasterizerState> m_RasterizerState;
    D3D11_RASTERIZER_DESC m_RasterizerDesc;
    CKBOOL m_RasterizerStateUpToDate = TRUE;

    D3D_FEATURE_LEVEL m_FeatureLevel;
    D3D11_VIEWPORT m_Viewport;
    CKBOOL m_AllowTearing;
    CKDWORD m_CurrentVShader = -1;
    CKDWORD m_CurrentPShader = -1;
    CKDWORD m_FVF = 0;

    CKDX11ConstantBufferDesc m_ConstantBuffer;
    CKBOOL m_ConstantBufferUpToDate;
    std::unordered_map<CKDWORD, CKDWORD> m_VertexShaderMap;
    
    VxDirectXData m_DirectXData;
    //    //----------------------------------------------------
    //--- Index buffer filled when drawing primitives
    CKDX11IndexBufferDesc *m_IndexBuffer[2]; // Clip/unclipped
    CKDWORD m_DynamicIndexBufferCounter = 0;
    // CKDWORD m_DirectVertexBufferCounter = 0;

    // CKDX11VertexBufferDesc *m_DynamicVertexBuffer[DYNAMIC_VBO_COUNT] = {nullptr};
    CKDX11IndexBufferDesc *m_DynamicIndexBuffer[DYNAMIC_IBO_COUNT] = {nullptr};
    ConstantBufferStruct m_CBuffer;

    volatile CKBOOL m_InCreateDestroy;
    std::string m_OriginalTitle;
    //-------------------------------------------------
    //--- to avoid redoundant calls to SetVertexShader & SetStreamSource :
    //--- a cache with the current vertex format and source VB
    // CKDWORD m_CurrentVertexShaderCache;
    // CKDWORD m_CurrentVertexFormatCache;
    // LPDIRECT3DVERTEXBUFFER9 m_CurrentVertexBufferCache;
    // CKDWORD m_CurrentVertexSizeCache;

    XBitArray m_StateCacheHitMask;
    XBitArray m_StateCacheMissMask;

    CKDX11Rasterizer *m_Owner;
};
