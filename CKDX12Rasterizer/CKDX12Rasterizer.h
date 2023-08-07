#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "CKRasterizer.h"
#include "CKDX12RasterizerCommon.h"
#include "CKDX12DynamicUploadHeap.h"
#include "CKDX12DynamicDescriptorHeap.h"

#include <vector>
#include <deque>
#include <memory>

#include <Windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>

#ifdef _DEBUG
    #include <dxgidebug.h>
#endif

#include <string>
#include <unordered_map>
#include <XBitArray.h>

#include "FlexibleVertexFormat.h"
using Microsoft::WRL::ComPtr;

class CKDX12Rasterizer : public CKRasterizer
{
public:
	CKDX12Rasterizer(void);
	virtual ~CKDX12Rasterizer(void);

	virtual XBOOL Start(WIN_HANDLE AppWnd);
	virtual void Close(void);

public:
    ComPtr<IDXGIFactory4> m_Factory;
    CKBOOL m_TearingSupport = FALSE;
    CKBOOL m_FlipPresent = FALSE;
    std::string m_DXGIVersionString = "1.1";
};

class CKDX12RasterizerDriver : public CKRasterizerDriver
{
public:
    CKDX12RasterizerDriver(CKDX12Rasterizer *rst);
    virtual ~CKDX12RasterizerDriver();

    //--- Contexts
    virtual CKRasterizerContext *CreateContext();
    virtual CKBOOL DestroyContext(CKRasterizerContext *Context);
    
    CKBOOL InitializeCaps(ComPtr<IDXGIAdapter1> Adapter, ComPtr<IDXGIOutput> Output, bool HighPerformance);

public:
    CKBOOL m_Inited;
    ComPtr<IDXGIAdapter1> m_Adapter;
    ComPtr<IDXGIOutput> m_Output;
    DXGI_ADAPTER_DESC1 m_AdapterDesc;
    DXGI_OUTPUT_DESC m_OutputDesc;
};

class CKDX12RasterizerContext;

struct CKDX12LightConstant
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

    CKDX12LightConstant() {}
    CKDX12LightConstant(CKLightData ld) :
        type(ld.Type), ambient(ld.Ambient), diffuse(ld.Diffuse), specular(ld.Specular),
        direction(VxVector4(ld.Direction.x, ld.Direction.y, ld.Direction.z, 0.)),
        position(VxVector4(ld.Position.x, ld.Position.y, ld.Position.z, 1.)), range(ld.Range), falloff(ld.Falloff),
        theta(ld.InnerSpotCone), phi(ld.OuterSpotCone), a0(ld.Attenuation0), a1(ld.Attenuation1), a2(ld.Attenuation2)
    {
    }
};

enum TexOp // for CKDX12TexCombinatorConstant::op
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

enum TexArg // for CKDX12TexCombinatorConstant::cargs / aargs
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

struct CKDX12TexCombinatorConstant
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

    static CKDX12TexCombinatorConstant make(TexOp cop, TexArg ca1, TexArg ca2, TexArg ca3, TexOp aop, TexArg aa1,
                                         TexArg aa2, TexArg aa3, TexArg dest, CKDWORD constant)
    {
        CKDWORD op = cop | (aop << 4) | ((dest == TexArg::temp) << 31);
        CKDWORD cargs = ca1 | (ca2 << 8) | (ca3 << 16);
        CKDWORD aargs = aa1 | (aa2 << 8) | (aa3 << 16);
        return CKDX12TexCombinatorConstant{op, cargs, aargs, constant};
    }
};

static constexpr int MAX_TEX_STAGES = 2;
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
    uint32_t FVF = 0;
    uint32_t _padding1 = 1234;
    uint32_t _padding2 = 1234;
    uint32_t _padding3 = 1234;
    uint32_t TextureTransformFlags[4];
    VxMatrix TexTransformMatrix[MAX_TEX_STAGES];
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

static constexpr uint32_t FFLG_FOGEN = 1U << 31;
static constexpr int MAX_ACTIVE_LIGHTS = 16;
typedef struct PSConstantBufferStruct
{
    CKMaterialData Material;
    uint32_t AlphaFlags = 0;
    float AlphaThreshold = 0.0f;
    uint32_t GlobalLightSwitches = 0; // a
    VxVector ViewPosition;
    uint32_t FVF = 0; // a
    uint32_t FogFlags = 0;
    float FogStart;
    float FogEnd;
    float FogDensity;
    VxColor FogColor;
    float DepthRange[2];
    uint32_t NullTextureMask;
    float _padding2;
} PSConstantBufferStruct;

typedef struct PSLightConstantBufferStruct {
    CKDX12LightConstant Lights[MAX_ACTIVE_LIGHTS];
} PSLightConstantBufferStruct;

typedef struct PSTexCombinatorConstantBufferStruct
{
    CKDX12TexCombinatorConstant TexCombinator[MAX_TEX_STAGES];
} PSTexCombinatorConstantBufferStruct;

struct FVFResource
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
    D3D12_SHADER_BYTECODE shader;
};

typedef struct CKDX12VertexBufferDesc : public CKVertexBufferDesc
{
public:
    //ComPtr<ID3D12Resource> DxResource;
    D3D12_VERTEX_BUFFER_VIEW DxView;
    CKDX12VertexBufferDesc() { ZeroMemory(&DxView, sizeof(D3D12_VERTEX_BUFFER_VIEW)); }
    CKDX12VertexBufferDesc(const CKVertexBufferDesc& desc): CKVertexBufferDesc(desc)
    {
        ZeroMemory(&DxView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
    }
    CKDX12VertexBufferDesc(const CKVertexBufferDesc &desc, const CKDX12AllocatedResource& resource):
        CKVertexBufferDesc(desc)
    {
        ZeroMemory(&DxView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
        DxView.BufferLocation = resource.GPUAddress;
        DxView.SizeInBytes = desc.m_VertexSize * desc.m_MaxVertexCount;
        DxView.StrideInBytes = desc.m_VertexSize;
        CPUAddress = resource.CPUAddress;
    }
    void *CPUAddress = nullptr;
    /*virtual CKBOOL Create(CKDX12RasterizerContext *ctx);
    virtual void *Lock();
    virtual void Unlock();*/
} CKDX12VertexBufferDesc;

typedef struct CKDX12IndexBufferDesc : public CKIndexBufferDesc
{
public:
    //ComPtr<ID3D12Resource> DxResource;
    D3D12_INDEX_BUFFER_VIEW DxView;
    CKDX12IndexBufferDesc() { ZeroMemory(&DxView, sizeof(D3D12_INDEX_BUFFER_VIEW)); }
    CKDX12IndexBufferDesc(const CKIndexBufferDesc &desc) : CKIndexBufferDesc(desc)
    {
        ZeroMemory(&DxView, sizeof(D3D12_INDEX_BUFFER_VIEW));
    }
    CKDX12IndexBufferDesc(const CKIndexBufferDesc &desc, const CKDX12AllocatedResource &resource) :
        CKIndexBufferDesc(desc)
    {
        ZeroMemory(&DxView, sizeof(D3D12_INDEX_BUFFER_VIEW));
        DxView.BufferLocation = resource.GPUAddress;
        DxView.SizeInBytes = resource.Size;
        DxView.Format = DXGI_FORMAT_R16_UINT;
        CPUAddress = resource.CPUAddress;
    }
    void *CPUAddress = nullptr;
    /*virtual CKBOOL Create(CKDX12RasterizerContext *ctx);
    virtual void *Lock();
    virtual void Unlock();*/
} CKDX12IndexBufferDesc;

//typedef struct CKDX12ConstantBufferDesc
//{
//public:
//    ComPtr<ID3D12Resource> DxResource;
//    D3D12_CONSTANT_BUFFER_VIEW_DESC DxView;
//    CKDX12ConstantBufferDesc() { ZeroMemory(&DxView, sizeof(D3D12_INDEX_BUFFER_VIEW)); }
//    virtual CKBOOL Create(CKDX12RasterizerContext *ctx, UINT size);
//    virtual void *Lock();
//    virtual void Unlock();
//} CKDX12ConstantBufferDesc;

class CKDX12RasterizerContext : public CKRasterizerContext
{
public:
    //--- Construction/destruction
    CKDX12RasterizerContext();
    virtual ~CKDX12RasterizerContext();
    void resize_buffers();
    void toggle_fullscreen(bool fullscreen);

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
    virtual CKBOOL LoadSprite(CKDWORD Sprite, const VxImageDescEx &SurfDesc);

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
    CKBOOL CreateSpriteNPOT(CKDWORD Sprite, CKSpriteDesc *DesiredFormat);
    CKBOOL InternalSetRenderState(VXRENDERSTATETYPE State, CKDWORD Value);
    
    void SetTitleStatus(const char *fmt, ...);
    HRESULT CreateCommandQueue();
    HRESULT CreateSwapchain(WIN_HANDLE Window, int Width, int Height);
    HRESULT CreateDescriptorHeap();
    HRESULT CreateFrameResources();
    HRESULT CreateSyncObject();
    HRESULT CreateRootSignature();
    void PrepareShaders();
    HRESULT CreatePSOs();
    void CreateResources();

    HRESULT WaitForGpu();
    HRESULT MoveToNextFrame();

    CKBOOL TriangleFanToList(CKWORD VOffset, CKDWORD VCount, std::vector<CKWORD> &strip_index);
    CKBOOL TriangleFanToList(CKWORD *indices, int count, std::vector<CKWORD> &strip_index);

    CKDWORD GetDynamicIndexBuffer(CKDWORD IndexCount, CKDWORD AddKey);
    
    CKBOOL InternalDrawPrimitive();
    CKBOOL InternalDrawPrimitiveIndexed();
#ifdef _NOD3DX
    CKBOOL LoadSurface(const D3DSURFACE_DESC &ddsd, const D3DLOCKED_RECT &LockRect, const VxImageDescEx &SurfDesc);
#endif
    
public:
    // We overload the meaning of FrameCount to mean both the maximum
    // number of frames that will be queued to the GPU at a time, as well as the number
    // of back buffers in the DXGI swap chain. For the majority of applications, this
    // is convenient and works well. However, there will be certain cases where an
    // application may want to queue up more frames than there are back buffers
    // available.
    // It should be noted that excessive buffering of frames dependent on user input
    // may result in noticeable latency in your app.
    static const UINT m_BufferedFrameCount = 2;
    static const UINT m_BackBufferCount = 2;

    VxDirectXData m_DirectXData;
    
    CKBOOL m_InCreateDestroy = TRUE;
    std::string m_OriginalTitle;

    XBitArray m_StateCacheHitMask;
    XBitArray m_StateCacheMissMask;

    CKDX12Rasterizer *m_Owner;
    BOOL m_Inited = FALSE;

    // Pipeline objects
    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
    UINT m_RTVDescriptorSize;
    ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
    UINT m_DSVDescriptorSize;
    std::unique_ptr<CKDX12DynamicDescriptorHeap> m_VSCBVHeap;
    std::unique_ptr<CKDX12DynamicUploadHeap> m_VSCBHeap;

    std::unique_ptr<CKDX12DynamicUploadHeap> m_VBHeap;
    std::unique_ptr<CKDX12DynamicUploadHeap> m_IBHeap;

    ComPtr<ID3D12Resource> m_RenderTargets[m_BackBufferCount];
    ComPtr<ID3D12Resource> m_DepthStencils[m_BackBufferCount];
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12CommandAllocator> m_CommandAllocators[m_BufferedFrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    std::vector<ID3D12CommandList *> m_PendingCommandList;
    
    std::unordered_map<DWORD, FVFResource> m_FVFResources;
    std::unordered_map<DWORD, ComPtr<ID3D12PipelineState>> m_PipelineState;
    CD3DX12_VIEWPORT m_Viewport;
    CD3DX12_RECT m_ScissorRect;

    /*CKDX12ConstantBufferDesc m_VSConstantBuffer;
    CKDX12ConstantBufferDesc m_PSConstantBuffer;
    CKDX12ConstantBufferDesc m_PSLightConstantBuffer;
    CKDX12ConstantBufferDesc m_PSTexCombinatorConstantBuffer;*/
    VSConstantBufferStruct m_VSCBuffer;
    PSConstantBufferStruct m_PSCBuffer;
    PSLightConstantBufferStruct m_PSLightCBuffer;
    PSTexCombinatorConstantBufferStruct m_PSTexCombinatorCBuffer;
    CKBOOL m_VSConstantBufferUpToDate = FALSE;
    CKBOOL m_PSConstantBufferUpToDate = FALSE;
    CKBOOL m_PSLightConstantBufferUpToDate = FALSE;
    CKBOOL m_PSTexCombinatorConstantBufferUpToDate = FALSE;
    
    std::deque<CKDX12VertexBufferDesc> m_VertexBufferSubmitted;
    size_t m_VertexBufferSubmittedCount[m_BufferedFrameCount] = {0};
    std::deque<CKDX12IndexBufferDesc> m_IndexBufferSubmitted;
    size_t m_IndexBufferSubmittedCount[m_BufferedFrameCount] = {0};

#if defined(DEBUG) || defined(_DEBUG)
    bool m_CmdListClosed = true;
#endif
    // Sync objects
    UINT m_FrameIndex = 0;
    HANDLE m_FenceEvent;
    ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_FenceValues[m_BufferedFrameCount];

    asio::thread_pool m_ThreadPool;
};
