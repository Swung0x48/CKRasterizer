#include "CKDX12Rasterizer.h"
#include "CKDX12RasterizerCommon.h"
#include "EnumMaps.h"

#include "VShader2DColor1.h"
#include "VShader2DColor2.h"
#include "VShaderColor.h"
#include "VShaderColor1Tex1.h"
#include "VShaderColor1Tex2.h"
#include "VShaderColor2Tex1.h"
#include "VShaderColor2Tex2.h"
#include "VShaderNormalTex1.h"
#include "VShaderNormalTex2.h"
#include "VShaderTex.h"
#include "PShader.h"
// TODO: Remove it soon
#include "PShaderSimple.h"

#include <algorithm>

#if defined(DEBUG) || defined(_DEBUG)
    #define STATUS 1
    //#define LOGGING 1
    #define CONSOLE 1
    #define CMDLIST 0
#endif

#if LOGGING || CONSOLE
#include <conio.h>
static bool step_mode = false;
#endif

#if STATUS
static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;
#endif

void flag_toggle(uint32_t *state_dword, uint32_t flag, bool enabled)
{
    if (enabled)
        *state_dword |= flag;
    else
        *state_dword &= ~0U ^ flag;
}

void InverseMatrix(VxMatrix& result, const VxMatrix &m)
{
    using namespace DirectX;

    // XMFLOAT4X4 src((const float*) &m);
    XMMATRIX srcmat = XMLoadFloat4x4((XMFLOAT4X4*)&m);

    XMMATRIX invmat = XMMatrixInverse(nullptr, srcmat);

    XMFLOAT4X4 *dest = (XMFLOAT4X4 *)&result;
    XMStoreFloat4x4(dest, invmat);
}

CKDX12RasterizerContext::CKDX12RasterizerContext() { CKRasterizerContext::CKRasterizerContext(); }
CKDX12RasterizerContext::~CKDX12RasterizerContext()
{
}

void CKDX12RasterizerContext::SetTitleStatus(const char *fmt, ...)
{
    std::string ts;
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        va_list argsx;
        va_copy(argsx, args);
        ts.resize(vsnprintf(NULL, 0, fmt, argsx) + 1);
        va_end(argsx);
        vsnprintf(ts.data(), ts.size(), fmt, args);
        va_end(args);
        ts = m_OriginalTitle + " | " + ts;
    }
    else
        ts = m_OriginalTitle;

    SetWindowTextA(GetAncestor((HWND)m_Window, GA_ROOT), ts.c_str());
}

HRESULT CKDX12RasterizerContext::CreateCommandQueue() {
    HRESULT hr;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    D3DCall(
        m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateSwapchain(WIN_HANDLE Window, int Width, int Height)
{
    HRESULT hr;
    BOOL allowTearing = FALSE;
    ComPtr<IDXGIFactory5> factory5;
    hr = m_Owner->m_Factory.As(&factory5);
    if (SUCCEEDED(hr))
    {
        // TODO: Put into context creation
        hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
        allowTearing = allowTearing && SUCCEEDED(hr);
    }

    DXGI_SWAP_CHAIN_DESC1 scd{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC scfd{};
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    scd.Width = 0;
    scd.Height = 0;
    scd.Format = format;
    scd.Stereo = FALSE;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = m_BackBufferCount;
    scd.Scaling = DXGI_SCALING_NONE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    scd.Flags =
        // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | (allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

    scfd.RefreshRate.Numerator = 0;
    scfd.RefreshRate.Denominator = 0;
    scfd.Scaling = DXGI_MODE_SCALING_STRETCHED;
    scfd.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    scfd.Windowed = TRUE;
    ComPtr<IDXGISwapChain1> swapChain;
    D3DCall(m_Owner->m_Factory->CreateSwapChainForHwnd(
        m_CommandQueue.Get(),
        (HWND)Window,
        &scd,
        &scfd,
        nullptr,
        &swapChain)
    );
    D3DCall(swapChain.As(&m_SwapChain));
    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateDescriptorHeap() {
    HRESULT hr;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_BackBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    D3DCall(m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap)));
    if (SUCCEEDED(hr))
        m_RTVDescriptorSize = m_Device->
            GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = m_BackBufferCount;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    D3DCall(m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));
    if (SUCCEEDED(hr))
        m_DSVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateFrameResources()
{
    HRESULT hr = S_OK;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each frame.
    for (UINT i = 0; i < m_BackBufferCount; i++)
    {
        D3DCall(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_RTVDescriptorSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_RESOURCE_DESC dsDesc;
    ZeroMemory(&dsDesc, sizeof(D3D12_RESOURCE_DESC));
    auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE clearValueDs;
    ZeroMemory(&clearValueDs, sizeof(D3D12_CLEAR_VALUE));
    clearValueDs.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValueDs.DepthStencil.Stencil = 0;
    clearValueDs.DepthStencil.Depth = 1.0f;
    for (UINT i = 0; i < m_BackBufferCount; ++i)
    {
        // TODO: Create DSV resources
        dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dsDesc.Alignment = 0;
        dsDesc.Width = m_Width;
        dsDesc.Height = m_Height;
        dsDesc.DepthOrArraySize = 1;
        dsDesc.MipLevels = 1;
        dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsDesc.SampleDesc.Count = 1;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3DCall(m_Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &dsDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                          &clearValueDs, IID_PPV_ARGS(&m_DepthStencils[i])));
        //D3DCall(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_DepthStencils[i])));
        m_Device->CreateDepthStencilView(m_DepthStencils[i].Get(), nullptr, dsvHandle);
        dsvHandle.Offset(1, m_DSVDescriptorSize);
    }

    // and a command allocator...
    for (UINT i = 0; i < m_FrameInFlightCount; i++)
    {
        D3DCall(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocators[i])));
#if defined(DEBUG) || defined(_DEBUG)
        static char buf[50];
        sprintf(buf, "cmdAllocator%u", i);
        WCHAR wstr[100];
        memset(wstr, 0, sizeof(wstr));
        MultiByteToWideChar(CP_ACP, 0, buf, strlen(buf), wstr, 100);
        m_CommandAllocators[i]->SetName(wstr);
#endif
    }

    // ...also, a command list for each of them
    for (UINT i = 0; i < m_BackBufferCount; ++i)
    {
        D3DCall(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            m_CommandAllocators[i].Get(), nullptr, 
            IID_PPV_ARGS(&m_CommandList)));
        

        D3DCall(m_CommandList->Close());
    }

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateSyncObject() {
    HRESULT hr;
    D3DCall(m_Device->CreateFence(m_FenceValues[m_FrameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
    m_FenceValues[m_FrameIndex]++;

    // Create an event handle to use for frame synchronization.
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_FenceEvent == nullptr)
    {
        D3DCall(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Wait for the command list to execute; we are reusing the same command
    // list in our main loop but for now, we just want to wait for setup to
    // complete before continuing.
    D3DCall(WaitForGpu());
    return hr;
}

HRESULT CKDX12RasterizerContext::CreateRootSignature() {
    HRESULT hr;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned
    // will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
    // Allow input layout and deny uneccessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    
    hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
    if (hr == E_INVALIDARG)
        fprintf(stderr, "%s\n", error->GetBufferPointer());
    else
        D3DCall(hr);
    D3DCall(m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                                IID_PPV_ARGS(&m_RootSignature)));

    return hr;
}

void CKDX12RasterizerContext::PrepareShaders() {
    DWORD fvf = CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_TEX1;
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    D3D12_SHADER_BYTECODE shader{g_VShader2DColor1, sizeof(g_VShader2DColor1)};
    FVFResource res{elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_RASTERPOS | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShader2DColor2, sizeof(g_VShader2DColor2)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX1;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderNormalTex1, sizeof(g_VShaderNormalTex1)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX2;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderNormalTex2, sizeof(g_VShaderNormalTex2)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX1;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderColor1Tex1, sizeof(g_VShaderColor1Tex1)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX2;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderColor1Tex2, sizeof(g_VShaderColor1Tex2)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX1;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderColor2Tex1, sizeof(g_VShaderColor2Tex1)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_SPECULAR | CKRST_VF_TEX2;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderColor2Tex2, sizeof(g_VShaderColor2Tex2)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_TEX1;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderTex, sizeof(g_VShaderTex)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;

    fvf = CKRST_VF_POSITION | CKRST_VF_DIFFUSE;
    FVF::CreateInputLayoutFromFVF(fvf, elements);
    shader = {g_VShaderColor, sizeof(g_VShaderColor)};
    res = {elements, shader};
    m_FVFResources[fvf] = res;
}

HRESULT CKDX12RasterizerContext::CreatePSOs() {
    HRESULT hr;
    assert(!m_FVFResources.empty());
    for (auto &item : m_FVFResources)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { item.second.input_layout.data(), item.second.input_layout.size() };
        psoDesc.pRootSignature = m_RootSignature.Get();
        psoDesc.VS = item.second.shader;
        psoDesc.PS = {g_PShaderSimple, sizeof(g_PShaderSimple)};
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        // Stencil test parameters
        psoDesc.DepthStencilState.StencilEnable = TRUE;
        psoDesc.DepthStencilState.StencilReadMask = 0xFF;
        psoDesc.DepthStencilState.StencilWriteMask = 0xFF;

        // Stencil operations if pixel is front-facing
        psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
        psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        // Stencil operations if pixel is back-facing
        psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_DECR;
        psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        ID3D12PipelineState *state = nullptr;
        D3DCall(m_Device->CreateGraphicsPipelineState(&psoDesc, 
            IID_PPV_ARGS(&state)));
        m_PipelineState[item.first] = state;
    }

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateResources()
{
    HRESULT hr;
    const size_t size = 1024;
    m_VSCBHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device,
                                                           D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * size);
    m_CBV_SRV_Heap = std::make_unique<CKDX12DynamicDescriptorHeap>(size, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_Device);
    m_VBHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device, size, false);
    m_IBHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device, size, false);
    m_TextureHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device, size, false);
    m_DynamicVBHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device, size, false);
    m_DynamicIBHeap = std::make_unique<CKDX12DynamicUploadHeap>(true, m_Device, size, false);

    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.pDevice = m_Device.Get();
    desc.pAdapter = static_cast<CKDX12RasterizerDriver *>(m_Driver)->m_Adapter.Get();
    D3DCall(D3D12MA::CreateAllocator(&desc, &m_Allocator));
    return hr;
}

HRESULT CKDX12RasterizerContext::WaitForGpu()
{
    HRESULT hr;
    // Schedule a Signal command in the queue.
    D3DCall(m_CommandQueue->Signal(m_Fence.Get(), m_FenceValues[m_FrameIndex]));

    // Wait until the fence has been processed.
    D3DCall(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
    WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_FenceValues[m_FrameIndex]++;
    return hr;
}

HRESULT CKDX12RasterizerContext::MoveToNextFrame()
{
    HRESULT hr;
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_FenceValues[m_FrameIndex];
    D3DCall(m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue));

    // Update the frame index.
    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();

    auto completedValue = m_Fence->GetCompletedValue();
    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (completedValue < m_FenceValues[m_FrameIndex])
    {
        D3DCall(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    m_CBV_SRV_Heap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_VSCBHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_VBHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_IBHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_TextureHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_DynamicVBHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    m_DynamicIBHeap->FinishFrame(m_FenceValues[m_FrameIndex] + 1, completedValue);
    
    // Release resources no longer in use.
    m_VertexBufferSubmitted[m_FrameIndex].clear();
    m_IndexBufferSubmitted[m_FrameIndex].clear();
    
    // Set the fence value for the next frame.
    m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
    return hr;
}

CKBOOL CKDX12RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
                                       CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
    m_InCreateDestroy = TRUE;
#if (LOGGING) || (CONSOLE)
    AllocConsole();
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif
    m_OriginalTitle.resize(GetWindowTextLengthA(GetAncestor((HWND)Window, GA_ROOT)) + 1);
    GetWindowTextA(GetAncestor((HWND)Window, GA_ROOT), m_OriginalTitle.data(), m_OriginalTitle.size());
    while (m_OriginalTitle.back() == '\0')
        m_OriginalTitle.pop_back();

    
    memset(m_FenceValues, 0, sizeof(m_FenceValues));

    HRESULT hr;
    m_Window = Window;
    m_PosX = PosX;
    m_PosY = PosY;
    m_Width = Width;
    m_Height = Height;
    m_Fullscreen = Fullscreen;
    m_RefreshRate = RefreshRate;
    m_ZBpp = Zbpp;
    m_StencilBpp = StencilBpp;

    auto *driver = static_cast<CKDX12RasterizerDriver *>(m_Driver);
    D3DCall(D3D12CreateDevice(driver->m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
    D3DCall(CreateCommandQueue());
    D3DCall(CreateSwapchain(Window, m_Width, m_Height));
    D3DCall(CreateDescriptorHeap());
    D3DCall(CreateFrameResources());

    // TODO: Load initial assets here (shaders etc.)
    D3DCall(CreateRootSignature());
    PrepareShaders();
    D3DCall(CreatePSOs());
    D3DCall(CreateResources());

    D3DCall(CreateSyncObject());
    
    SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, 1);
    SetRenderState(VXRENDERSTATE_LOCALVIEWER, 1);
    SetRenderState(VXRENDERSTATE_COLORVERTEX, 0);

    m_InCreateDestroy = FALSE;
    m_Inited = TRUE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX12RasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    CKRECT Rect;
    if (m_Window)
    {
        VxGetWindowRect(m_Window, &Rect);
        WIN_HANDLE Parent = VxGetParent(m_Window);
        VxScreenToClient(Parent, reinterpret_cast<CKPOINT *>(&Rect));
        VxScreenToClient(Parent, reinterpret_cast<CKPOINT *>(&Rect.right));
    }
    return TRUE;
}
CKBOOL CKDX12RasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount,
                                      CKRECT *rects)
{
#if LOGGING
    fprintf(stderr, "Clear\n");
#endif
    if (!m_SceneBegined)
        BeginScene();
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_RTVDescriptorSize);
    if (Flags & CKRST_CTXCLEAR_COLOR)
    {
        VxColor c(Ccol);
        m_CommandList->ClearRenderTargetView(rtvHandle, (const float*)&c, 0, nullptr);
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_DSVDescriptorSize);
    UINT dsClearFlag = 0;
    if (Flags & CKRST_CTXCLEAR_DEPTH)
        dsClearFlag |= D3D12_CLEAR_FLAG_DEPTH;
    if (Flags & CKRST_CTXCLEAR_STENCIL)
        dsClearFlag |= D3D12_CLEAR_FLAG_STENCIL;
    if (dsClearFlag)
        m_CommandList->ClearDepthStencilView(dsvHandle, (D3D12_CLEAR_FLAGS)dsClearFlag, Z, Stencil, 0,
                                             nullptr);
    
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::BackToFront(CKBOOL vsync)
{
    if (m_SceneBegined)
        EndScene();
#if LOGGING
    fprintf(stderr, "BackToFront\n");
#endif
#if STATUS
    SetTitleStatus("D3D12 | DXGI %s | batch stats: direct %d, vb %d, vbib %d",
                   m_Owner->m_DXGIVersionString.c_str(), directbat, vbbat, vbibbat);

    directbat = 0;
    vbbat = 0;
    vbibbat = 0;
#endif

    HRESULT hr;
    //D3DCall(m_SwapChain->Present(1, 0));
    D3DCall(m_SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));
    D3DCall(MoveToNextFrame());
    return SUCCEEDED(hr);
}

CKBOOL CKDX12RasterizerContext::BeginScene() {
    if (m_SceneBegined)
        return TRUE;
#if LOGGING
    fprintf(stderr, "BeginScene\n");
#endif
    m_SceneBegined = TRUE;
    HRESULT hr;
    D3DCall(m_CommandAllocators[m_FrameIndex]->Reset());
    D3DCall(m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), nullptr));
#if CMDLIST
    fprintf(stderr, "m_CommandList->Reset() %u\n", m_SwapChain->GetCurrentBackBufferIndex());
#if defined DEBUG || defined _DEBUG
    m_CmdListClosed = false;
#endif // DEBUG || defined _DEBUG
#endif
    const auto transitionToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &transitionToRenderTarget);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_RTVDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_DSVDescriptorSize);
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    std::vector<ID3D12DescriptorHeap *> ppHeaps;
    for (auto i = 0; i < m_CBV_SRV_Heap->m_Heaps.size(); ++i)
    {
        ppHeaps.emplace_back(m_CBV_SRV_Heap->m_Heaps[i].m_Heap.Get());
    }
    m_CommandList->SetDescriptorHeaps(ppHeaps.size(), ppHeaps.data());
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::EndScene() {
    if (!m_SceneBegined)
        return TRUE;

#if LOGGING
    fprintf(stderr, "EndScene\n");
#endif

    HRESULT hr = S_OK;

    const auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &transitionToPresent);
    D3DCall(m_CommandList->Close());
#if CMDLIST
    fprintf(stderr, "m_CommandList->Close() %u\n", m_SwapChain->GetCurrentBackBufferIndex());
#if defined DEBUG || defined _DEBUG
    m_CmdListClosed = true;
#endif // DEBUG || defined _DEBUG
#endif
    assert(m_CmdListClosed);
    ID3D12CommandList *list = m_CommandList.Get();
    m_CommandQueue->ExecuteCommandLists(1, &list);
    //m_PendingCommandList.emplace_back(m_CommandList.Get());
    //m_CommandQueue->ExecuteCommandLists(m_PendingCommandList.size(), m_PendingCommandList.data());
    //m_PendingCommandList.clear();

    m_SceneBegined = FALSE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX12RasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    if (Light >= MAX_ACTIVE_LIGHTS)
        return FALSE;
    m_PSLightConstantBufferUpToDate = FALSE;
    m_CurrentLightData[Light] = *data;
    bool enabled = (m_PSLightCBuffer.Lights[Light].type & LFLG_LIGHTEN);
    m_PSLightCBuffer.Lights[Light] = *data;
    flag_toggle(&m_PSLightCBuffer.Lights[Light].type, LFLG_LIGHTEN, enabled);
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    if (Light >= MAX_ACTIVE_LIGHTS)
        return FALSE;
    m_PSLightConstantBufferUpToDate = FALSE;
    flag_toggle(&m_PSLightCBuffer.Lights[Light].type, LFLG_LIGHTEN, Enable);
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::SetMaterial(CKMaterialData *mat)
{
    m_CurrentMaterialData = *mat;
    m_PSConstantBufferUpToDate = FALSE;
    m_PSCBuffer.Material = *mat;
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::SetViewport(CKViewportData *data) {
#if LOGGING
    fprintf(stderr, "SetViewport\n");
#endif
    if (!m_SceneBegined)
        BeginScene();
    m_ViewportData = *data;

    m_Viewport.TopLeftX = (FLOAT)data->ViewX;
    m_Viewport.TopLeftY = (FLOAT)data->ViewY;
    m_Viewport.Width = (FLOAT)data->ViewWidth;
    m_Viewport.Height = (FLOAT)data->ViewHeight;
    m_Viewport.MaxDepth = 1.0f;
    m_Viewport.MinDepth = 0.0f;
    m_CommandList->RSSetViewports(1, &m_Viewport);

    m_ScissorRect.left = data->ViewX;
    m_ScissorRect.top = data->ViewY;
    m_ScissorRect.right = data->ViewWidth;
    m_ScissorRect.bottom = data->ViewHeight;
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    m_VSConstantBufferUpToDate = FALSE;
    m_VSCBuffer.ViewportMatrix = VxMatrix::Identity();
    float(*m)[4] = (float(*)[4]) & m_VSCBuffer.ViewportMatrix;
    m[0][0] = 2. / data->ViewWidth;
    m[1][1] = 2. / data->ViewHeight;
    m[2][2] = 0;
    m[3][0] = -(-2. * data->ViewX + data->ViewWidth) / data->ViewWidth;
    m[3][1] = (-2. * data->ViewY + data->ViewHeight) / data->ViewHeight;
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat) {
    CKDWORD UnityMatrixMask = 0;
    switch (Type)
    {
        case VXMATRIX_PROJECTION:
            {
                float(*m)[4] = (float(*)[4]) & Mat;
                float A = m[2][2];
                float B = m[3][2];
                m_PSCBuffer.DepthRange[0] = -B / A;
                m_PSCBuffer.DepthRange[1] = B / (1 - A); // for eye-distance fog calculation
                m_PSConstantBufferUpToDate = FALSE;
                break;
            }
        case VXMATRIX_TEXTURE0:
        case VXMATRIX_TEXTURE1:
        case VXMATRIX_TEXTURE2:
        case VXMATRIX_TEXTURE3:
        case VXMATRIX_TEXTURE4:
        case VXMATRIX_TEXTURE5:
        case VXMATRIX_TEXTURE6:
        case VXMATRIX_TEXTURE7:
            {
                UnityMatrixMask = TEXTURE0_TRANSFORM << (Type - TEXTURE1_TRANSFORM);
                CKDWORD tex = Type - VXMATRIX_TEXTURE0;
                Vx3DTransposeMatrix(m_VSCBuffer.TexTransformMatrix[tex], Mat);
                // m_VSCBuffer.TexTransformMatrix[tex] = Mat;
                m_VSConstantBufferUpToDate = FALSE;
                break;
            }
        default:
            break;
    }
    auto ret = CKRasterizerContext::SetTransformMatrix(Type, Mat);
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= UnityMatrixMask;
    }
    else
    {
        m_UnityMatrixMask &= ~UnityMatrixMask;
    }
    m_VSConstantBufferUpToDate = FALSE;
    return ret;
}

CKBOOL CKDX12RasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    if (m_StateCache[State].Flag)
        return TRUE;

    if (m_StateCache[State].Valid && m_StateCache[State].Value == Value)
    {
        ++m_RenderStateCacheHit;
        return TRUE;
    }

#if LOGGING && LOG_RENDERSTATE
    if (State == VXRENDERSTATE_SRCBLEND || State == VXRENDERSTATE_DESTBLEND)
    fprintf(stderr, "set render state %s -> %x, currently %x %s\n", rstytostr(State), Value,
            m_StateCache[State].Valid ? m_StateCache[State].Value : m_StateCache[State].DefaultValue,
            m_StateCache[State].Valid ? "" : "[Invalid]");
#endif
    ++m_RenderStateCacheMiss;
    m_StateCache[State].Value = Value;
    m_StateCache[State].Valid = 1;

    if (State < m_StateCacheMissMask.Size() && m_StateCacheMissMask.IsSet(State))
        return FALSE;

    return InternalSetRenderState(State, Value);
}

CKBOOL CKDX12RasterizerContext::InternalSetRenderState(VXRENDERSTATETYPE State, CKDWORD Value) { return TRUE; }

CKBOOL CKDX12RasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}
CKBOOL CKDX12RasterizerContext::SetTexture(CKDWORD Texture, int Stage) { return TRUE; }

CKBOOL CKDX12RasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    return TRUE;
}
CKBOOL CKDX12RasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    return TRUE;
}
CKBOOL CKDX12RasterizerContext::SetPixelShader(CKDWORD PShaderIndex) { return TRUE; }

CKBOOL CKDX12RasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}
CKBOOL CKDX12RasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}

CKBOOL CKDX12RasterizerContext::TriangleFanToList(CKWORD VOffset, CKDWORD VCount,
                                                                  std::vector<CKWORD>& strip_index)
{
    strip_index.clear();
    strip_index.reserve(VCount * 3);
    // Center at VOffset
    for (CKWORD i = 2; i < VCount; ++i)
    {
        strip_index.emplace_back(VOffset);
        strip_index.emplace_back(i - 1 + VOffset);
        strip_index.emplace_back(i + VOffset);
    }
    return !strip_index.empty();
}

CKBOOL CKDX12RasterizerContext::TriangleFanToList(CKWORD *indices, int count,
                                                  std::vector<CKWORD> &strip_index)
{
    if (!indices)
        return FALSE;
    CKWORD center = indices[0];
    for (CKWORD i = 2; i < count; ++i)
    {
        strip_index.emplace_back(center);
        strip_index.emplace_back(indices[i - 1]);
        strip_index.emplace_back(indices[i]);
    }
    return !strip_index.empty();
}

static CKDWORD ib_idx = 0;
CKDWORD CKDX12RasterizerContext::GetDynamicIndexBuffer(CKDWORD IndexCount, CKDWORD AddKey)
{
    ib_idx %= m_IndexBuffers.Size();
    CKIndexBufferDesc *ib = m_IndexBuffers[ib_idx];
    if (!ib || ib->m_MaxIndexCount < IndexCount)
    {
        if (ib)
        {
            delete ib;
            m_IndexBuffers[ib_idx] = NULL;
        }

        CKIndexBufferDesc nib;
        nib.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        nib.m_MaxIndexCount = IndexCount;
        nib.m_CurrentICount = 0;
        if (AddKey != 0)
            nib.m_Flags |= CKRST_VB_SHARED;
        CreateObject(ib_idx, CKRST_OBJ_INDEXBUFFER, &nib);
    }
    return ib_idx++;
}

CKBOOL CKDX12RasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount,
                                              VxDrawPrimitiveData *data)
{
    HRESULT hr;
#if LOGGING
    fprintf(stderr, "DrawPrimitive\n");
#endif
#if STATUS
    ++directbat;
#endif
    if (!m_SceneBegined)
        BeginScene();

    std::vector<CKWORD> ib;
    switch (pType)
    {
        case VX_TRIANGLELIST:
            /*if (indices)
            {
                ib.resize(indexcount);
                memcpy(ib.data(), indices, sizeof(CKWORD) * indexcount);
            }*/
            break;
        case VX_TRIANGLEFAN:
            return TRUE;
            if (indices)
                TriangleFanToList(indices, indexcount, ib);
            else
            {
                CKWORD voffset = 0;
                TriangleFanToList(voffset, indexcount, ib);
            }
            break;
#if defined(DEBUG) || defined(_DEBUG)
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            return TRUE;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            return TRUE;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            return TRUE;
#endif
        default:
#if defined(DEBUG) || defined(_DEBUG)
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
#endif
            return TRUE;
    }
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    m_CommandList->SetPipelineState(m_PipelineState[vertexFormat].Get());
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (!m_VSConstantBufferUpToDate)
    {
        UpdateMatrices(WORLD_TRANSFORM);
        UpdateMatrices(VIEW_TRANSFORM);
        Vx3DTransposeMatrix(m_VSCBuffer.WorldMatrix, m_WorldMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ViewMatrix, m_ViewMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ProjectionMatrix, m_ProjectionMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.TotalMatrix, m_TotalMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldMatrix, m_WorldMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldViewMatrix, m_ModelViewMatrix);
        auto res = m_VSCBHeap->Allocate(sizeof(VSConstantBufferStruct), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(res.CPUAddress, &m_VSCBuffer, sizeof(VSConstantBufferStruct));
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle;
        HRESULT hr;
        D3DCall(m_CBV_SRV_Heap->CreateConstantBufferView(res, handle));
        m_CommandList->SetGraphicsRootDescriptorTable(0, handle);
        m_VSConstantBufferUpToDate = TRUE;
    }

    CKBOOL clip = FALSE;
    if ((data->Flags & CKRST_DP_DOCLIP))
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 1);
        clip = TRUE;
    }
    else
    {
        SetRenderState(VXRENDERSTATE_CLIPPING, 0);
    }
    
    CKDWORD VB = GetDynamicVertexBuffer(vertexFormat, data->VertexCount, vertexSize, clip);
    auto *vbo = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);
    CKBYTE *pData = nullptr;
    CKDWORD vbase = 0;
    if (vbo->m_CurrentVCount + data->VertexCount <= vbo->m_MaxVertexCount)
    {
        pData = (CKBYTE *)vbo->CPUAddress + vertexSize * vbo->m_CurrentVCount;
        vbase = vbo->m_CurrentVCount;
        vbo->m_CurrentVCount += data->VertexCount;
    } else
    {
        pData = (CKBYTE *)vbo->CPUAddress;
        vbase = 0;
        vbo->m_CurrentVCount = data->VertexCount;
    }
    
    CKRSTLoadVertexBuffer((CKBYTE *)pData, vertexFormat, vertexSize, data);

    m_CommandList->IASetVertexBuffers(0, 1, &vbo->DxView);

    int ibcount = (pType == VX_TRIANGLEFAN) ? ib.size() : indexcount;
    assert(ibcount > 0);
    if (indices || pType == VX_TRIANGLEFAN)
    {
        auto res = m_DynamicIBHeap->Allocate(ibcount * sizeof(CKWORD));
        CKIndexBufferDesc desc;
        desc.m_MaxIndexCount = indexcount;
        desc.m_CurrentICount = 0;
        desc.m_Flags = 0;
        CKDX12IndexBufferDesc ibo(desc, res);
        //m_IndexBufferSubmitted.emplace_back(desc, res);
        //++m_IndexBufferSubmittedCount[m_FrameIndex];
        assert(pType == VX_TRIANGLELIST || pType == VX_TRIANGLEFAN);
        if (pType == VX_TRIANGLELIST)
            memcpy(ibo.CPUAddress, indices, indexcount * sizeof(CKWORD));
        else if (pType == VX_TRIANGLEFAN)
            memcpy(ibo.CPUAddress, ib.data(), ib.size() * sizeof(CKWORD));
        m_CommandList->IASetIndexBuffer(&ibo.DxView);
        m_CommandList->DrawIndexedInstanced(ibcount, 1, desc.m_CurrentICount, vbase, 0);
    } else
    {
        m_CommandList->DrawInstanced(data->VertexCount, 1, vbase, 0);
    }
    /*asio::post(m_ThreadPool,
               [this, index_vec, vb = std::move(vb)]() 
    {
        
    });*/
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD StartVIndex,
                                                CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if STATUS
    ++vbbat;
#endif
#if LOGGING
    fprintf(stderr, "DrawPrimitiveVB\n");
#endif
    std::vector<CKWORD> ib;
    switch (pType)
    {
        case VX_TRIANGLELIST:
            break;
        case VX_TRIANGLEFAN:
            if (indices)
                TriangleFanToList(indices, indexcount, ib);
            else
            {
                CKWORD voffset = 0;
                TriangleFanToList(voffset, indexcount, ib);
            }
            break;
#if defined(DEBUG) || defined(_DEBUG)
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            return TRUE;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            return TRUE;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            return TRUE;
#endif
        default:
#if defined(DEBUG) || defined(_DEBUG)
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
#endif
            return TRUE;
    }
    auto *vbo = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);
    m_CommandList->IASetVertexBuffers(0, 1, &vbo->DxView);

    m_CommandList->SetPipelineState(m_PipelineState[vbo->m_VertexFormat].Get());
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (!m_VSConstantBufferUpToDate)
    {
        UpdateMatrices(WORLD_TRANSFORM);
        UpdateMatrices(VIEW_TRANSFORM);
        Vx3DTransposeMatrix(m_VSCBuffer.WorldMatrix, m_WorldMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ViewMatrix, m_ViewMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ProjectionMatrix, m_ProjectionMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.TotalMatrix, m_TotalMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldMatrix, m_WorldMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldViewMatrix, m_ModelViewMatrix);
        auto res = m_VSCBHeap->Allocate(sizeof(VSConstantBufferStruct), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(res.CPUAddress, &m_VSCBuffer, sizeof(VSConstantBufferStruct));
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle;
        HRESULT hr;
        D3DCall(m_CBV_SRV_Heap->CreateConstantBufferView(res, handle));
        m_CommandList->SetGraphicsRootDescriptorTable(0, handle);
        m_VSConstantBufferUpToDate = TRUE;
    }

    int ibcount = (pType == VX_TRIANGLEFAN) ? ib.size() : indexcount;
    assert(ibcount > 0);
    if (indices || pType == VX_TRIANGLEFAN)
    {
        auto res = m_DynamicIBHeap->Allocate(ibcount * sizeof(CKWORD));
        CKIndexBufferDesc desc;
        desc.m_MaxIndexCount = indexcount;
        desc.m_CurrentICount = 0;
        desc.m_Flags = 0;
        CKDX12IndexBufferDesc ibo(desc, res);
        // m_IndexBufferSubmitted.emplace_back(desc, res);
        //++m_IndexBufferSubmittedCount[m_FrameIndex];
        assert(pType == VX_TRIANGLELIST || pType == VX_TRIANGLEFAN);
        if (pType == VX_TRIANGLELIST)
            memcpy(ibo.CPUAddress, indices, indexcount * sizeof(CKWORD));
        else if (pType == VX_TRIANGLEFAN)
            memcpy(ibo.CPUAddress, ib.data(), ib.size() * sizeof(CKWORD));
        m_CommandList->IASetIndexBuffer(&ibo.DxView);
        m_CommandList->DrawIndexedInstanced(ibcount, 1, desc.m_CurrentICount, StartVIndex, 0);
    }
    else
    {
        m_CommandList->DrawInstanced(VertexCount, 1, StartVIndex, 0);
    }
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                                  CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if STATUS
    ++vbibbat;
#endif
#if LOGGING
    fprintf(stderr, "DrawPrimitiveVBIB\n");
#endif
    switch (pType)
    {
        case VX_TRIANGLELIST:
            break;
        case VX_TRIANGLEFAN:
            break;
#if defined(DEBUG) || defined(_DEBUG)
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            return TRUE;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            return TRUE;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            return TRUE;
#endif
        default:
#if defined (DEBUG) || defined(_DEBUG)
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
#endif
            return TRUE;
    }
    auto *vbo = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!vbo)
        return FALSE;
    auto size = m_IndexBuffers.Size();
    auto *ibo = static_cast<CKDX12IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!ibo)
        return FALSE;
//#if defined(DEBUG) || defined(_DEBUG)
//    static char buf[50];
//    sprintf(buf, "VB %u\0", VB);
//    WCHAR wstr[100];
//    memset(wstr, 0, sizeof(wstr));
//    MultiByteToWideChar(CP_ACP, 0, buf, strlen(buf), wstr, 100);
//    vbo->DxResource->SetName(wstr);
//
//    sprintf(buf, "IB %u\0", IB);
//    memset(wstr, 0, sizeof(wstr));
//    MultiByteToWideChar(CP_ACP, 0, buf, strlen(buf), wstr, 100);
//    ibo->DxResource->SetName(wstr);
//#endif
    m_VertexBufferSubmitted[m_FrameIndex].emplace_back(*vbo);
    m_IndexBufferSubmitted[m_FrameIndex].emplace_back(*ibo);
    m_CommandList->SetPipelineState(m_PipelineState[vbo->m_VertexFormat].Get());
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &vbo->DxView);
    m_CommandList->IASetIndexBuffer(&ibo->DxView);

    if (!m_VSConstantBufferUpToDate)
    {
        UpdateMatrices(WORLD_TRANSFORM);
        UpdateMatrices(VIEW_TRANSFORM);
        Vx3DTransposeMatrix(m_VSCBuffer.WorldMatrix, m_WorldMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ViewMatrix, m_ViewMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.ProjectionMatrix, m_ProjectionMatrix);
        Vx3DTransposeMatrix(m_VSCBuffer.TotalMatrix, m_TotalMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldMatrix, m_WorldMatrix);
        InverseMatrix(m_VSCBuffer.TransposedInvWorldViewMatrix, m_ModelViewMatrix);
        auto res = 
            m_VSCBHeap->Allocate(sizeof(VSConstantBufferStruct), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        memcpy(res.CPUAddress, &m_VSCBuffer, sizeof(VSConstantBufferStruct));
        CD3DX12_GPU_DESCRIPTOR_HANDLE handle;
        HRESULT hr;
        D3DCall(m_CBV_SRV_Heap->CreateConstantBufferView(res, handle));
        m_CommandList->SetGraphicsRootDescriptorTable(0, handle);
        m_VSConstantBufferUpToDate = TRUE;
    }
#if defined(DEBUG) || defined(_DEBUG)
    WCHAR *stats = nullptr;
    m_Allocator->BuildStatsString(&stats, TRUE);
    fprintf(stdout, "%ls\n", stats);
    m_Allocator->FreeStatsString(stats);
#endif
    m_CommandList->DrawIndexedInstanced(Indexcount, 1, StartIndex, MinVIndex, 0);
    return TRUE;
}
CKBOOL CKDX12RasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    int result;

    if (ObjIndex >= m_Textures.Size())
        return FALSE;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
            result = CreateSpriteNPOT(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXBUFFER:
            result = CreateVertexBuffer(ObjIndex, static_cast<CKVertexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_INDEXBUFFER:
            result = CreateIndexBuffer(ObjIndex, static_cast<CKIndexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXSHADER:
            result = CreateVertexShader(ObjIndex, static_cast<CKVertexShaderDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_PIXELSHADER:
            result = CreatePixelShader(ObjIndex, static_cast<CKPixelShaderDesc *>(DesiredFormat));
            break;
        default:
            return 0;
    }
    return result;
}

CKBOOL CKDX12RasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    if (Texture >= m_Textures.Size())
        return FALSE;
    auto *desc = static_cast<CKDX12TextureDesc *>(m_Textures[Texture]);
    if (!desc)
        return FALSE;
    //auto *desc = m_Textures[Texture];
    VxImageDescEx dst;
    dst.Size = sizeof(VxImageDescEx);
    ZeroMemory(&dst.Flags, sizeof(VxImageDescEx) - sizeof(dst.Size));
    dst.Width = SurfDesc.Width;
    dst.Height = SurfDesc.Height;
    dst.BitsPerPixel = 32;
    dst.BytesPerLine = 4 * SurfDesc.Width;
    dst.AlphaMask = 0xFF000000;
    dst.RedMask = 0x0000FF;
    dst.GreenMask = 0x00FF00;
    dst.BlueMask = 0xFF0000;
    dst.Image = new uint8_t[dst.Width * dst.Height * (dst.BitsPerPixel / 8)];
    VxDoBlit(SurfDesc, dst);
    if (!(SurfDesc.AlphaMask || SurfDesc.Flags >= _DXT1))
        VxDoAlphaBlit(dst, 255);

    // TODO: create texture and SRV here...
    HRESULT hr;
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = desc->Format.Width;
    textureDesc.Height = desc->Format.Height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    D3DCall(m_Allocator->CreateResource(&allocDesc, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                        desc->Allocation.ReleaseAndGetAddressOf(), IID_NULL, nullptr));
    desc->DefaultResource.Reset();
    desc->DefaultResource = desc->Allocation->GetResource();
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(desc->DefaultResource.Get(), 0, 1);
    auto res = m_TextureHeap->Allocate(uploadBufferSize);
    desc->UploadResource = res;
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = dst.Image;
    textureData.RowPitch = desc->Format.Width * (dst.BitsPerPixel / 8);
    textureData.SlicePitch = textureData.RowPitch * desc->Format.Height;
    UpdateSubresources(m_CommandList.Get(), desc->DefaultResource.Get(), res.pBuffer.Get(), res.Offset,
        0, 1, &textureData);
    delete dst.Image;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle;
    CKDX12AllocatedResource defaultRes{desc->DefaultResource, 0, (size_t)uploadBufferSize};
    D3DCall(m_CBV_SRV_Heap->CreateShaderResourceView(desc->DefaultResource.Get(), &srvDesc, handle));
    m_CommandList->SetGraphicsRootDescriptorTable(1, handle);
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(desc->DefaultResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_CommandList->ResourceBarrier(1, &transition);

    return TRUE;
}

CKBOOL CKDX12RasterizerContext::LoadSprite(CKDWORD Sprite, const VxImageDescEx &SurfDesc)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    LoadTexture(spr->Textures.Front().IndexTexture, SurfDesc);
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}
CKBOOL CKDX12RasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face,
                                                 CKBOOL GenerateMipMap)
{
    return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face, GenerateMipMap);
}
CKBOOL CKDX12RasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size())
        return FALSE;
    CKSpriteDesc *spr = m_Sprites[Sprite];
    VxDrawPrimitiveData pd{};
    pd.VertexCount = 4;
    pd.Flags = CKRST_DP_VCT;
    VxVector4 p[4] = {VxVector4(dst->left, dst->top, 0, 1.), VxVector4(dst->right, dst->top, 0, 1.),
                      VxVector4(dst->right, dst->bottom, 0, 1.), VxVector4(dst->left, dst->bottom, 0, 1.)};
    CKDWORD c[4] = {~0U, ~0U, ~0U, ~0U};
    Vx2DVector t[4] = {Vx2DVector(src->left, src->top), Vx2DVector(src->right, src->top),
                       Vx2DVector(src->right, src->bottom), Vx2DVector(src->left, src->bottom)};
    for (int i = 0; i < 4; ++i)
    {
        t[i].x /= spr->Format.Width;
        t[i].y /= spr->Format.Height;
    }
    pd.PositionStride = sizeof(VxVector4);
    pd.ColorStride = sizeof(CKDWORD);
    pd.TexCoordStride = sizeof(Vx2DVector);
    pd.PositionPtr = p;
    pd.ColorPtr = c;
    pd.TexCoordPtr = t;
    static const CKWORD idx[6] = {0, 1, 2, 0, 2, 3};
    SetTexture(spr->Textures.Front().IndexTexture);
    SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    DrawPrimitive(VX_TRIANGLELIST, const_cast<CKWORD *>(idx), 6, &pd);
    return TRUE;
}
int CKDX12RasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyToMemoryBuffer(rect, buffer, img_desc);
}
int CKDX12RasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyFromMemoryBuffer(rect, buffer, img_desc);
}
CKBOOL CKDX12RasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)
{
    return CKRasterizerContext::SetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}
CKBOOL CKDX12RasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)
{
    return CKRasterizerContext::GetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

void *CKDX12RasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
                                                CKRST_LOCKFLAGS Lock)
{
    if (VB > m_VertexBuffers.Size())
        return nullptr;
    auto *desc = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);

    assert(StartVertex + VertexCount <= desc->m_MaxVertexCount);
    desc->LockStart = StartVertex;
    desc->LockCount = VertexCount;
    desc->LockFlags = Lock;

    return desc->CPUAddress;
}

CKBOOL CKDX12RasterizerContext::UnlockVertexBuffer(CKDWORD VB) {
    if (VB > m_VertexBuffers.Size())
        return FALSE;
    auto *desc = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);
    if (!desc)
        return FALSE;

    desc->m_Flags = CKRST_VB_VALID;
    if (desc->m_Flags & CKRST_VB_DYNAMIC)
        return TRUE;
    if (desc->Filled)
    {
        fprintf(stderr, "UnlockVertexBuffer %lu is filled.\n", VB);
        return TRUE;
    }
    if (desc->LockCount == 0)
    {
        // Skip locking a 0-sized lock.
        return TRUE;
    }

    // We can do a UPLOAD_HEAP -> DEFAULT_HEAP copy here.
    // We won't actually unlock here.
    // It's okay to leave resources locked in D3D12
    HRESULT hr;
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc->m_VertexSize * desc->m_MaxVertexCount);
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    D3DCall(m_Allocator->CreateResource(&allocDesc, &resDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                desc->Allocation.ReleaseAndGetAddressOf(),
                                IID_NULL, nullptr));
    desc->DefaultResource.Reset();
    desc->DefaultResource = desc->Allocation->GetResource();
    CKDWORD offset = desc->LockStart * desc->m_VertexSize;
    CKDWORD size = desc->LockCount * desc->m_VertexSize;
    // Do the copy ("locking")
    m_CommandList->CopyBufferRegion(desc->DefaultResource.Get(), offset, desc->UploadResource.pBuffer.Get(),
                                    desc->UploadResource.Offset + offset, size);

    const auto transition = CD3DX12_RESOURCE_BARRIER::Transition(desc->DefaultResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                                 D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_CommandList->ResourceBarrier(1, &transition);
    desc->DxView.BufferLocation = desc->DefaultResource->GetGPUVirtualAddress();
    return TRUE;
}

void *CKDX12RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    if (IB > m_IndexBuffers.Size())
        return nullptr;
    auto *desc = static_cast<CKDX12IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!desc)
        return nullptr;
    desc->LockStart = StartIndex;
    desc->LockCount = IndexCount;
    desc->LockFlags = Lock;

    return desc->CPUAddress;
}

CKBOOL CKDX12RasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB > m_IndexBuffers.Size())
        return FALSE;
    auto *desc = static_cast<CKDX12IndexBufferDesc *>(m_IndexBuffers[IB]);
    if (!desc)
        return FALSE;
    if (desc->m_Flags & CKRST_VB_DYNAMIC)
        return TRUE;

    desc->m_Flags = CKRST_VB_VALID;
    if (desc->LockCount == 0)
    {
        // Skip locking a 0-sized lock.
        return TRUE;
    }

    // We can do a UPLOAD_HEAP -> DEFAULT_HEAP copy here.
    // We won't actually unlock here.
    // It's okay to leave resources locked in D3D12
    HRESULT hr;
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(desc->m_MaxIndexCount * sizeof(CKWORD));
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
    D3DCall(m_Allocator->CreateResource(&allocDesc, &resDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                        desc->Allocation.ReleaseAndGetAddressOf(), IID_NULL, nullptr));
    desc->DefaultResource.Reset();
    desc->DefaultResource = desc->Allocation->GetResource();
    CKDWORD offset = desc->LockStart * sizeof(CKWORD);
    CKDWORD size = desc->LockCount * sizeof(CKWORD);
    m_CommandList->CopyBufferRegion(desc->DefaultResource.Get(), offset, desc->UploadResource.pBuffer.Get(),
                                    desc->UploadResource.Offset + offset, size);
    const auto transition =
        CD3DX12_RESOURCE_BARRIER::Transition(desc->DefaultResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                             D3D12_RESOURCE_STATE_INDEX_BUFFER);
    m_CommandList->ResourceBarrier(1, &transition);
    desc->DxView.BufferLocation = desc->DefaultResource->GetGPUVirtualAddress();

    return TRUE;
}

CKBOOL CKDX12RasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat) { return TRUE; }

CKBOOL CKDX12RasterizerContext::CreateSpriteNPOT(CKDWORD Sprite, CKSpriteDesc *DesiredFormat)
{
    if (Sprite >= (CKDWORD)m_Sprites.Size() || !DesiredFormat)
        return FALSE;
    if (m_Sprites[Sprite])
        delete m_Sprites[Sprite];
    m_Sprites[Sprite] = new CKSpriteDesc();
    CKSpriteDesc *spr = m_Sprites[Sprite];
    spr->Flags = DesiredFormat->Flags;
    spr->Format = DesiredFormat->Format;
    spr->MipMapCount = DesiredFormat->MipMapCount;
    spr->Owner = m_Driver->m_Owner;
    CKSPRTextInfo ti;
    ti.IndexTexture = m_Driver->m_Owner->CreateObjectIndex(CKRST_OBJ_TEXTURE);
    CreateObject(ti.IndexTexture, CKRST_OBJ_TEXTURE, DesiredFormat);
    spr->Textures.PushBack(ti);
    return TRUE;
}

bool operator==(const CKVertexShaderDesc &a, const CKVertexShaderDesc& b)
{
    return a.m_FunctionSize == b.m_FunctionSize &&
        (a.m_Function == b.m_Function || 
            memcmp(a.m_Function, b.m_Function, a.m_FunctionSize) == 0);
}

CKBOOL CKDX12RasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat) { return TRUE; }

bool operator==(const CKPixelShaderDesc& a, const CKPixelShaderDesc& b)
{
    return a.m_FunctionSize == b.m_FunctionSize &&
        (a.m_Function == b.m_Function || memcmp(a.m_Function, b.m_Function, a.m_FunctionSize) == 0);
}

CKBOOL CKDX12RasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat) { return TRUE; }

bool operator==(const CKVertexBufferDesc& a, const CKVertexBufferDesc& b){
    return a.m_CurrentVCount == b.m_CurrentVCount && a.m_Flags == b.m_Flags &&
        a.m_MaxVertexCount == b.m_MaxVertexCount && a.m_VertexFormat == b.m_VertexFormat &&
        a.m_VertexSize == b.m_VertexSize;
}

CKBOOL CKDX12RasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return FALSE;

    CKDX12DynamicUploadHeap *heap = nullptr;
    if (DesiredFormat->m_Flags & CKRST_VB_DYNAMIC)
        heap = m_DynamicVBHeap.get();
    else
        heap = m_VBHeap.get();
    auto res = heap->Allocate(DesiredFormat->m_VertexSize * DesiredFormat->m_MaxVertexCount);
    auto *desc = new CKDX12VertexBufferDesc(*DesiredFormat, res);
    delete m_VertexBuffers[VB];
    
    m_VertexBuffers[VB] = desc;
    return TRUE;
}

bool operator==(const CKIndexBufferDesc &a, const CKIndexBufferDesc &b)
{
    return a.m_Flags == b.m_Flags && a.m_CurrentICount == b.m_CurrentICount &&
        a.m_MaxIndexCount == b.m_MaxIndexCount;
}

CKBOOL CKDX12RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat) {
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;

    auto res = m_IBHeap->Allocate(DesiredFormat->m_MaxIndexCount * sizeof(CKWORD));
    auto *desc = new CKDX12IndexBufferDesc(*DesiredFormat, res);
    delete m_IndexBuffers[IB];

    m_IndexBuffers[IB] = desc;
    return TRUE;
}
