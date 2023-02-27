#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#define DYNAMIC_VBO_COUNT 64
#define DYNAMIC_IBO_COUNT 64

#include "CKRasterizer.h"
#include <Windows.h>
#include <d3d11.h>
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

static constexpr int MAX_ACTIVE_LIGHTS = 16;
static constexpr int MAX_TEX_STAGES = 2;
static constexpr int MAX_TEX_STAGES_PAD_TO_ALIGN = MAX_TEX_STAGES / 4 * 4 + ((MAX_TEX_STAGES % 4 == 0) ? 0 : 4);

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
    CKBOOL m_FlipPresent = FALSE;
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
    LPCSTR DxEntryPoint = "main";
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
    LPCSTR DxEntryPoint = "main";
    LPCSTR DxTarget = "ps_4_0";
    ComPtr<ID3DBlob> DxErrorMsgs;
    virtual CKBOOL Compile(CKDX11RasterizerContext *ctx);
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx);
    virtual void Bind(CKDX11RasterizerContext *ctx);
} CKDX11PixelShaderDesc;

struct CKDX11LightConstant
{
    uint32_t type; // highest bit as LIGHTEN
    float a0;
    float a1;
    float a2; // align
    VxColor ambient; // a
    VxColor diffuse; // a
    VxColor specular; // a
    VxVector4 direction; // a
    VxVector4 position; // a
    float range;
    float falloff;
    float theta;
    float phi; // a

    CKDX11LightConstant() {}
    CKDX11LightConstant(CKLightData ld) :
        type(ld.Type), ambient(ld.Ambient), diffuse(ld.Diffuse), specular(ld.Specular),
        direction(VxVector4(ld.Direction.x, ld.Direction.y, ld.Direction.z, 0.)),
        position(VxVector4(ld.Position.x, ld.Position.y, ld.Position.z, 1.)), range(ld.Range), falloff(ld.Falloff),
        theta(ld.InnerSpotCone), phi(ld.OuterSpotCone), a0(ld.Attenuation0), a1(ld.Attenuation1), a2(ld.Attenuation2)
    {
    }
};

enum TexOp // for CKDX11TexCombinatorConstant::op
{
    disable = 0x1,
    select1 = 0x2,
    select2 = 0x3,
    modulate = 0x4,
    modulate2 = 0x5,
    modulate4 = 0x6,
    add = 0x7,
    addbip = 0x8,
    addbip2 = 0x9,
    subtract = 0xa,
    addsmooth = 0xb,
    mixtexalp = 0xd,
    top_max = ~0UL
};

enum TexArg // for CKDX11TexCombinatorConstant::cargs / aargs
{
    diffuse = 0x0,
    current = 0x1,
    texture = 0x2,
    tfactor = 0x3,
    specular = 0x4,
    temp = 0x5,
    constant = 0x6,
    flag_cpl = 0x10,
    flag_alp = 0x20,
    tar_max = ~0UL
};

struct CKDX11TexCombinatorConstant
{
    CKDWORD op; // bit 0-3: color op, bit 4-7: alpha op, bit 31: dest
    CKDWORD cargs; // bit 0-7: arg1, bit 8-15: arg2, bit 16-23: arg3
    CKDWORD aargs; // ditto
    CKDWORD constant;

    void set_color_op(TexOp o)
    {
        op &= ~0x0fU;
        op |= o;
    }
    void set_alpha_op(TexOp o)
    {
        op &= ~0xf0U;
        op |= (o << 4);
    }
    void set_color_arg1(TexArg a)
    {
        cargs &= ~0x00ffU;
        cargs |= a;
    }
    void set_color_arg2(TexArg a)
    {
        cargs &= ~0xff00U;
        cargs |= (a << 8);
    }
    void set_alpha_arg1(TexArg a)
    {
        aargs &= ~0x00ffU;
        aargs |= a;
    }
    void set_alpha_arg2(TexArg a)
    {
        aargs &= ~0xff00U;
        aargs |= (a << 8);
    }

    TexArg dest() { return op & (1UL << 31) ? TexArg::temp : TexArg::current; }

    static CKDX11TexCombinatorConstant make(TexOp cop, TexArg ca1, TexArg ca2, TexArg ca3, TexOp aop, TexArg aa1,
                                         TexArg aa2, TexArg aa3, TexArg dest, CKDWORD constant)
    {
        CKDWORD op = cop | (aop << 4) | ((dest == TexArg::temp) << 31);
        CKDWORD cargs = ca1 | (ca2 << 8) | (ca3 << 16);
        CKDWORD aargs = aa1 | (aa2 << 8) | (aa3 << 16);
        return CKDX11TexCombinatorConstant{op, cargs, aargs, constant};
    }
};

static constexpr uint32_t AFLG_ALPHATESTEN = 0x10U;
typedef struct VSConstantBufferStruct
{
    VxMatrix WorldMatrix;
    VxMatrix ViewMatrix;
    VxMatrix ProjectionMatrix;
    VxMatrix TotalMatrix;
    VxMatrix ViewportMatrix;
    VxMatrix TransposedInvWorldMatrix;
    VxMatrix TransposedInvWorldViewMatrix;
    VxMatrix TexTransformMatrix[MAX_TEX_STAGES];
    uint32_t FVF = 0;
    uint32_t TextureTransformFlags[MAX_TEX_STAGES];
    uint32_t _padding1 = 1234;
} VSConstantBufferStruct;

static constexpr uint32_t LFLG_LIGHTEN = 1U << 31;

static constexpr uint32_t LSW_LIGHTINGEN = 1U << 0;
static constexpr uint32_t LSW_SPECULAREN = 1U << 1;
static constexpr uint32_t LSW_VRTCOLOREN = 1U << 2;

// per-texture vertex properties
static constexpr uint32_t TVP_TC_CSNORM = 0x01000000; // use camera space normal as input tex-coords
static constexpr uint32_t TVP_TC_CSVECP = 0x02000000; // use camera space position ......
static constexpr uint32_t TVP_TC_CSREFV = 0x04000000; // use camera space reflect vector ......
static constexpr uint32_t TVP_TC_TRANSF = 0x08000000; // tex-coords should be transformed by its matrix
static constexpr uint32_t TVP_TC_PROJECTED = 0x10000000; // tex-coords should be projected

typedef struct PSConstantBufferStruct
{
    CKMaterialData Material;
    uint32_t AlphaFlags = 0;
    float AlphaThreshold = 0.0f;
    uint32_t GlobalLightSwitches = 0;
    VxVector ViewPosition;
    uint32_t FVF = 0;
    CKDX11LightConstant Lights[MAX_ACTIVE_LIGHTS];
    CKDX11TexCombinatorConstant TexCombinator[MAX_TEX_STAGES];
} PSConstantBufferStruct;

typedef struct CKDX11ConstantBufferDesc
{
public:
    ComPtr<ID3D11Buffer> DxBuffer;
    D3D11_BUFFER_DESC DxDesc;
    CKDX11ConstantBufferDesc() { ZeroMemory(&DxDesc, sizeof(D3D11_BUFFER_DESC)); }
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx, UINT size);
} CKDX11ConstantBufferDesc;

typedef struct CKDX11TextureDesc : CKTextureDesc
{
    D3D11_TEXTURE2D_DESC DxDesc;
    ComPtr<ID3D11Texture2D> DxTexture;
    ComPtr<ID3D11ShaderResourceView> DxSRV;
    CKDX11TextureDesc(CKTextureDesc *desc);
    virtual CKBOOL Create(CKDX11RasterizerContext *ctx, void *data);
    void Bind(CKDX11RasterizerContext *ctx, int stage);
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
    CKBOOL m_FlipPresent;
    CKDWORD m_CurrentVShader = -1;
    CKDWORD m_CurrentPShader = -1;
    // CKDWORD m_FVF = 0;

    CKDX11ConstantBufferDesc m_VSConstantBuffer;
    CKBOOL m_VSConstantBufferUpToDate;
    CKDX11ConstantBufferDesc m_PSConstantBuffer;
    CKBOOL m_PSConstantBufferUpToDate;
    std::unordered_map<CKDWORD, CKDWORD> m_VertexShaderMap;
    std::unordered_map<CKDWORD, std::vector<D3D11_INPUT_ELEMENT_DESC>> m_InputElementMap;
    std::unordered_map<CKDWORD, ComPtr<ID3D11InputLayout>> m_InputLayoutMap;
    
    VxDirectXData m_DirectXData;
    //    //----------------------------------------------------
    //--- Index buffer filled when drawing primitives
    CKDX11IndexBufferDesc *m_IndexBuffer[2]; // Clip/unclipped
    CKDWORD m_DynamicIndexBufferCounter = 0;
    // CKDWORD m_DirectVertexBufferCounter = 0;

    // CKDX11VertexBufferDesc *m_DynamicVertexBuffer[DYNAMIC_VBO_COUNT] = {nullptr};
    CKDX11IndexBufferDesc *m_DynamicIndexBuffer[DYNAMIC_IBO_COUNT] = {nullptr};
    VSConstantBufferStruct m_VSCBuffer;
    PSConstantBufferStruct m_PSCBuffer;

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
