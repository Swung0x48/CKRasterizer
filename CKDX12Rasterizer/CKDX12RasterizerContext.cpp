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
    #define LOGGING 1
#endif

#if LOGGING
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
    scd.BufferCount = 2;
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

    /*D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = m_BackBufferCount;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    D3DCall(m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));
    if (SUCCEEDED(hr))
        m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);*/

    return hr;
}

HRESULT CKDX12RasterizerContext::CreateFrameResources()
{
    HRESULT hr;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each frame.
    for (UINT i = 0; i < m_BackBufferCount; i++)
    {
        D3DCall(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i])));
        m_Device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_RTVDescriptorSize);
    }

    //CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    //for (UINT i = 0; i < m_BackBufferCount; ++i)
    //{
    //    // TODO: Create DSV resources
    //    //D3DCall(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_DepthStencils[i])));
    //    m_Device->CreateDepthStencilView(m_DepthStencils[i].Get(), nullptr, dsvHandle);
    //    dsvHandle.Offset(1, m_DSVDescriptorSize);
    //}

    // and a command allocator...
    for (UINT i = 0; i < m_BufferedFrameCount; i++)
    {
        D3DCall(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocators[i])));
#if defined(DEBUG) || defined(_DEBUG)
        static char buf[50];
        sprintf(buf, "cmdAllocator%u", i);
        WCHAR wstr[100];
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
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    CD3DX12_ROOT_PARAMETER1 rootParameters[1];

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    // Allow input layout and deny uneccessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    D3DCall(
        D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
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
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;

        ID3D12PipelineState *state = nullptr;
        D3DCall(m_Device->CreateGraphicsPipelineState(&psoDesc, 
            IID_PPV_ARGS(&state)));
        m_PipelineState[item.first] = state;
    }

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

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_Fence->GetCompletedValue() < m_FenceValues[m_FrameIndex])
    {
        D3DCall(m_Fence->SetEventOnCompletion(m_FenceValues[m_FrameIndex], m_FenceEvent));
        WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_FenceValues[m_FrameIndex] = currentFenceValue + 1;
    return hr;
}

CKBOOL CKDX12RasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
                                       CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
    m_InCreateDestroy = TRUE;
#if (LOGGING)
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

    D3DCall(CreateSyncObject());
    m_InCreateDestroy = FALSE;
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
        //const float color[] = {0.0f, 0.2f, 0.5f, 1.0f};
        m_CommandList->ClearRenderTargetView(rtvHandle, (const float*)&c, 0, nullptr);
    }
    /*CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_DSVDescriptorSize);
    UINT dsClearFlag = 0;
    if (Flags & CKRST_CTXCLEAR_DEPTH)
        dsClearFlag |= D3D12_CLEAR_FLAG_DEPTH;
    if (Flags & CKRST_CTXCLEAR_STENCIL)
        dsClearFlag |= D3D12_CLEAR_FLAG_STENCIL;
    if (dsClearFlag)
        m_CommandList->ClearDepthStencilView(dsvHandle, (D3D12_CLEAR_FLAGS)D3D12_CLEAR_FLAG_DEPTH, Z, Stencil, 0,
                                             nullptr);*/
    /*const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};*/
    
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
    // fprintf(stderr, "swap\n");
    SetTitleStatus("D3D12 | DXGI %s | batch stats: direct %d, vb %d, vbib %d",
                   m_Owner->m_DXGIVersionString.c_str(), directbat, vbbat, vbibbat);

    directbat = 0;
    vbbat = 0;
    vbibbat = 0;
#endif

    HRESULT hr;
    
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
    const auto transitionToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &transitionToRenderTarget);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex,
                                            m_RTVDescriptorSize);
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::EndScene() {
    if (!m_SceneBegined)
        return TRUE;

#if LOGGING
    fprintf(stderr, "EndScene\n");
#endif

    HRESULT hr;

    const auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &transitionToPresent);
    D3DCall(m_CommandList->Close());
    m_PendingCommandList.emplace_back(m_CommandList.Get());
    m_CommandQueue->ExecuteCommandLists(m_PendingCommandList.size(), m_PendingCommandList.data());
    m_PendingCommandList.clear();

    m_SceneBegined = FALSE;
    return SUCCEEDED(hr);
}

CKBOOL CKDX12RasterizerContext::SetLight(CKDWORD Light, CKLightData *data) { return TRUE; }

CKBOOL CKDX12RasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable) { return TRUE; }

CKBOOL CKDX12RasterizerContext::SetMaterial(CKMaterialData *mat) { return TRUE; }

CKBOOL CKDX12RasterizerContext::SetViewport(CKViewportData *data) { return TRUE; }

CKBOOL CKDX12RasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat) { return TRUE; }

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


//CKDX12IndexBufferDesc* CKDX12RasterizerContext::GenerateIB(void *indices, int indexCount, int *startIndex)
//{
//    ZoneScopedN(__FUNCTION__);
//    if (!indices)
//        return nullptr;
//    CKDX12IndexBufferDesc *ibo = nullptr;
//    void *pdata = nullptr;
//    auto iboid = m_DynamicIndexBufferCounter++;
//    if (m_DynamicIndexBufferCounter >= DYNAMIC_IBO_COUNT)
//        m_DynamicIndexBufferCounter = 0;
//    if (!m_DynamicIndexBuffer[iboid] || m_DynamicIndexBuffer[iboid]->m_MaxIndexCount < indexCount)
//    {
//        if (m_DynamicIndexBuffer[iboid])
//            delete m_DynamicIndexBuffer[iboid];
//        ibo = new CKDX12IndexBufferDesc;
//        ibo->m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
//        ibo->m_MaxIndexCount = indexCount + 100 < DEFAULT_VB_SIZE ? DEFAULT_VB_SIZE : indexCount + 100;
//        ibo->m_CurrentICount = 0;
//        if (!ibo->Create(this))
//        {
//            m_DynamicIndexBuffer[iboid] = nullptr;
//            return FALSE;
//        }
//        m_DynamicIndexBuffer[iboid] = ibo;
//    }
//    ibo = m_DynamicIndexBuffer[iboid];
//    if (indexCount + ibo->m_CurrentICount <= ibo->m_MaxIndexCount)
//    {
//        pdata = ibo->Lock(this, sizeof(CKWORD) * ibo->m_CurrentICount, sizeof(CKWORD) * indexCount, false);
//        *startIndex = ibo->m_CurrentICount;
//        ibo->m_CurrentICount += indexCount;
//    }
//    else
//    {
//        pdata = ibo->Lock(this, 0, sizeof(CKWORD) * indexCount, true);
//        *startIndex = 0;
//        ibo->m_CurrentICount = indexCount;
//    }
//    if (pdata)
//        std::memcpy(pdata, indices, sizeof(CKWORD) * indexCount);
//    ibo->Unlock(this);
//    return ibo;
//}
//
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

static size_t idx_vb = 0;
static size_t idx_ib = 0;
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
    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);
    size_t vbSize = vertexSize * data->VertexCount;
    auto vb = std::vector<CKBYTE>(vbSize);
    CKRSTLoadVertexBuffer(vb.data(), vertexFormat, vertexSize, data);

    std::vector<CKWORD> ib;
    switch (pType)
    {
        case VX_TRIANGLELIST:
            if (indices)
            {
                ib.resize(indexcount);
                memcpy(ib.data(), indices, sizeof(CKWORD) * indexcount);
            }
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
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            break;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            break;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            break;
        default:
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
            break;
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
//    auto vsize = m_VertexBuffers.Size();
//    auto isize = m_IndexBuffers.Size();
//    CKDWORD VB = idx_vb++;
//    idx_vb %= vsize;
//    CKVertexBufferDesc vdesc;
//    vdesc.m_VertexSize = vertexSize;
//    vdesc.m_MaxVertexCount = data->VertexCount;
//    vdesc.m_VertexFormat = vertexFormat;
//    CreateVertexBuffer(VB, &vdesc);
//    auto *vbo = static_cast<CKDX12VertexBufferDesc *>(m_VertexBuffers[VB]);
//    assert(vbo && vbo->m_MaxVertexCount >= data->VertexCount && vertexSize == vbo->m_VertexSize);
//    void *pbData = nullptr;
//    CKDWORD vbase = 0;
//    pbData = vbo->Lock();
//    vbase = vbo->m_CurrentVCount;
//    memcpy(pbData, vb.data(), vb.size() * vbo->m_VertexSize);
//    vbo->Unlock();
//    pbData = nullptr;
//
//    auto IB = GetDynamicIndexBuffer(ib.size(), clip);
//    auto *ibo = static_cast<CKDX12IndexBufferDesc *>(m_IndexBuffers[IB]);
//    assert(ibo && ibo->m_MaxIndexCount >= ib.size());
//    CKDWORD ibase = 0;
//    pbData = ibo->Lock();
//    vbase = ibo->m_CurrentICount;
//    memcpy(pbData, ib.data(), ib.size() * sizeof(CKWORD));
//    ibo->Unlock();
//    WaitForGpu();
//#if LOGGING
//    fprintf(stderr, "VB, IB: %d %d\n", VB, IB);
//#endif
//
//    // Populate command list
//    m_CommandList->SetPipelineState(m_PipelineState[vertexFormat].Get());
//    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//    auto to_vb = CD3DX12_RESOURCE_BARRIER::Transition(vbo->DxResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
//                                                      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
//    m_CommandList->ResourceBarrier(1, &to_vb);
//    m_CommandList->IASetVertexBuffers(0, 1, &vbo->DxView);
//    auto to_ib = CD3DX12_RESOURCE_BARRIER::Transition(vbo->DxResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
//                                                      D3D12_RESOURCE_STATE_INDEX_BUFFER);
//    m_CommandList->ResourceBarrier(1, &to_ib);
//    m_CommandList->IASetIndexBuffer(&ibo->DxView);
//    m_CommandList->DrawIndexedInstanced(ib.size(), 1, ibase, vbase, 0);

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
    switch (pType)
    {
        case VX_TRIANGLELIST:
            break;
        case VX_TRIANGLEFAN:
            break;
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            break;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            break;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            break;
        default:
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
            break;
    }
    return TRUE;
}

CKBOOL CKDX12RasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
                                                  CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if STATUS
    ++vbibbat;
#endif
    switch (pType)
    {
        case VX_TRIANGLELIST:
            break;
        case VX_TRIANGLEFAN:
            break;
        case VX_POINTLIST:
            fprintf(stderr, "Unhandled topology: VX_POINTLIST\n");
            break;
        case VX_LINELIST:
            fprintf(stderr, "Unhandled topology: VX_LINELIST\n");
            break;
        case VX_LINESTRIP:
            fprintf(stderr, "Unhandled topology: VX_LINESTRIP\n");
            break;
        default:
            fprintf(stderr, "Unhandled topology: 0x%x\n", pType);
            break;
    }
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
    return nullptr;
}
CKBOOL CKDX12RasterizerContext::UnlockVertexBuffer(CKDWORD VB) { return TRUE; }

void *CKDX12RasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    return nullptr;
}

CKBOOL CKDX12RasterizerContext::UnlockIndexBuffer(CKDWORD IB) { return TRUE; }

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

    auto *desc = new CKDX12VertexBufferDesc(*DesiredFormat);
    delete m_VertexBuffers[VB];
    auto succeeded = desc->Create(this);
    assert(succeeded);
    m_VertexBuffers[VB] = desc;
    return succeeded;
}

bool operator==(const CKIndexBufferDesc &a, const CKIndexBufferDesc &b)
{
    return a.m_Flags == b.m_Flags && a.m_CurrentICount == b.m_CurrentICount &&
        a.m_MaxIndexCount == b.m_MaxIndexCount;
}

CKBOOL CKDX12RasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat) {
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;

    auto *desc = new CKDX12IndexBufferDesc(*DesiredFormat);
    delete m_IndexBuffers[IB];
    auto succeeded = desc->Create(this);
    assert(succeeded);
    m_IndexBuffers[IB] = desc;
    return succeeded;
}
