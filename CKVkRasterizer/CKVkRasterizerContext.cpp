#include "CKVkRasterizer.h"
#include "CKVkBuffer.h"
#include "CKVkVertexBuffer.h"
#include "CKVkIndexBuffer.h"
#include "CKVkTexture.h"
#include "CKVkSampler.h"
#include "VkPipelineBuilder.h"
#include "ManagedVulkanPipeline.h"
#include "ResourceManagement.h"

#include <algorithm>

#define LOG_DRAWPRIMITIVE     0
#define LOG_DRAWPRIMITIVEVB   0
#define LOG_DRAWPRIMITIVEVBIB 0
#define DYNAMIC_VBO_COUNT 64
#define DYNAMIC_IBO_COUNT 64

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

static int directbat = 0;
static int vbbat = 0;
static int vbibbat = 0;

VxMatrix inv(const VxMatrix &m);

CKVkRasterizerContext::CKVkRasterizerContext()
{
}

CKVkRasterizerContext::~CKVkRasterizerContext()
{
    vkDeviceWaitIdle(vkdev);
    for (auto &s : sem_image_available)
        vkDestroySemaphore(vkdev, s, nullptr);
    for (auto &s : sem_render_finished)
        vkDestroySemaphore(vkdev, s, nullptr);
    for (auto &f : fence_cmdbuf_exec)
        vkDestroyFence(vkdev, f, nullptr);
    vkDestroyCommandPool(vkdev, cmdpool, nullptr);
    vkDestroyDescriptorPool(vkdev, descpool, nullptr);
    vkDestroyImageView(vkdev, depthv, nullptr);
    destroy_memory_image(vkdev, depthim);
    for (auto &fb : swchfb)
        vkDestroyFramebuffer(vkdev, fb, nullptr);
    for (auto &pl : pls)
        delete pl.second;
    delete root_pipeline;
    vkDestroyShaderModule(vkdev, vsh, nullptr);
    vkDestroyShaderModule(vkdev, fsh, nullptr);
    for (auto &ivw : swchivw)
        vkDestroyImageView(vkdev, ivw, nullptr);
    vkDestroySwapchainKHR(vkdev, vkswch, nullptr);
    vkDestroySurfaceKHR(vkinst, vksurface, nullptr);

    for (auto &mu : matubos)
    {
        mu.first->unmap();
        delete mu.first;
    }
    for (int i = 0; i < m_IndexBuffers.Size(); ++i)
        if (m_IndexBuffers[i])
            delete m_IndexBuffers[i];
    for (int i = 0; i < m_VertexBuffers.Size(); ++i)
        if (m_VertexBuffers[i])
            delete m_VertexBuffers[i];
    for (int i = 0; i < m_Textures.Size(); ++i)
        if (m_Textures[i])
            delete m_Textures[i];
    for (auto &dynib : dynibo)
        delete dynib;
    for (auto &dynvb : dynvbo)
        delete dynvb.second;
    for (auto &q : buf_deletion_queue)
        for (auto& b : q)
        {
            std::visit(overloaded {
                [](CKVkVertexBuffer *b) { delete b; },
                [](CKVkIndexBuffer *b) { delete b; }
            }, b);
        }

    m_DirtyRects.Clear();
    m_PixelShaders.Clear();
    m_VertexShaders.Clear();
    m_IndexBuffers.Clear();
    m_VertexBuffers.Clear();
    m_Sprites.Clear();
    m_Textures.Clear();
    delete smplr;
}

CKBOOL CKVkRasterizerContext::Create(WIN_HANDLE Window, int PosX, int PosY, int Width, int Height, int Bpp,
    CKBOOL Fullscreen, int RefreshRate, int Zbpp, int StencilBpp)
{
    auto surfacecinfo = make_vulkan_structure<VkWin32SurfaceCreateInfoKHR>({
        .hinstance=GetModuleHandle(NULL),
        .hwnd=(HWND)Window
    });
    if (VK_SUCCESS != vkCreateWin32SurfaceKHR(vkinst, &surfacecinfo, nullptr, &vksurface))
        return FALSE;
    //************** swapchain creation **************
    //TODO: check these in device selection
    VkSurfaceCapabilitiesKHR surfcaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkphydev, vksurface, &surfcaps);
    uint32_t fmtc = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkphydev, vksurface, &fmtc, nullptr);
    std::vector<VkSurfaceFormatKHR> surfacefmts(fmtc);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkphydev, vksurface, &fmtc, surfacefmts.data());
    uint32_t pmodc = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkphydev, vksurface, &pmodc, nullptr);
    std::vector<VkPresentModeKHR> presentmods(pmodc);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkphydev, vksurface, &pmodc, presentmods.data());
    VkSurfaceFormatKHR fmt = std::any_of(surfacefmts.begin(), surfacefmts.end(),
                                         [](VkSurfaceFormatKHR &p) { return p.format == VK_FORMAT_B8G8R8A8_SRGB &&
                                                                            p.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; })
                             ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : surfacefmts.front();
    VkPresentModeKHR pmod = std::find(presentmods.begin(), presentmods.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentmods.end() ?
                            VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D swpext = ~surfcaps.currentExtent.width ? surfcaps.currentExtent : VkExtent2D{(uint32_t)Width, (uint32_t)Height};
    uint32_t imgc = surfcaps.minImageCount + 1;
    if (surfcaps.maxImageCount > 0 && surfcaps.maxImageCount < imgc)
        imgc = surfcaps.maxImageCount;
    auto swchc = make_vulkan_structure<VkSwapchainCreateInfoKHR>({
        .surface=vksurface,
        .minImageCount=imgc,
        .imageFormat=fmt.format,
        .imageColorSpace=fmt.colorSpace,
        .imageExtent=swpext,
        .imageArrayLayers=1,
        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    });

    uint32_t qfidx[2] = {gqidx, pqidx};
    vkGetDeviceQueue(vkdev, gqidx, 0, &gfxq);
    vkGetDeviceQueue(vkdev, pqidx, 0, &prsq);
    if (qfidx[0] != qfidx[1])
    {
        swchc.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swchc.queueFamilyIndexCount = 2;
        swchc.pQueueFamilyIndices = qfidx;
    }
    else
        swchc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swchc.preTransform = surfcaps.currentTransform;
    swchc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swchc.presentMode = pmod;
    swchc.clipped = VK_TRUE;
    swchc.oldSwapchain = VK_NULL_HANDLE;

    if (VK_SUCCESS != vkCreateSwapchainKHR(vkdev, &swchc, nullptr, &vkswch))
        return FALSE;

    imgc = 0;
    vkGetSwapchainImagesKHR(vkdev, vkswch, &imgc, nullptr);
    swchi.resize(imgc);
    vkGetSwapchainImagesKHR(vkdev, vkswch, &imgc, swchi.data());
    swchifmt = fmt.format;
    swchiext = swpext;

    for (auto &img : swchi)
        swchivw.push_back(create_image_view(vkdev, img, swchifmt, VK_IMAGE_ASPECT_COLOR_BIT));

    auto shmodc = make_vulkan_structure<VkShaderModuleCreateInfo>(
    {
        .codeSize=get_resource_size("CKVKR_VERT_SHADER", "BUILTIN_VERTEX_SHADER"),
        .pCode=(uint32_t*)get_resource_data("CKVKR_VERT_SHADER", "BUILTIN_VERTEX_SHADER")
    });
    if (VK_SUCCESS != vkCreateShaderModule(vkdev, &shmodc, nullptr, &vsh))
        return FALSE;
    
    shmodc.codeSize = get_resource_size("CKVKR_FRAG_SHADER", "BUILTIN_FRAGMENT_SHADER");
    shmodc.pCode = (uint32_t*)get_resource_data("CKVKR_FRAG_SHADER", "BUILTIN_FRAGMENT_SHADER");
    if (VK_SUCCESS != vkCreateShaderModule(vkdev, &shmodc, nullptr, &fsh))
        return FALSE;

    auto vshstc = make_vulkan_structure<VkPipelineShaderStageCreateInfo>({
        .stage=VK_SHADER_STAGE_VERTEX_BIT,
        .module=vsh,
        .pName="main"
    });

    auto fshstc = make_vulkan_structure<VkPipelineShaderStageCreateInfo>({
        .stage=VK_SHADER_STAGE_FRAGMENT_BIT,
        .module=fsh,
        .pName="main"
    });

    VkAttachmentDescription coloratt {
        .flags=0,
        .format=swchifmt,
        .samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorattref {
        .attachment=0, //output location
        .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthatt {
        .flags=0,
        .format=VK_FORMAT_D32_SFLOAT,
        .samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthattref {
        .attachment=1,
        .layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorattref;
    subpass.pDepthStencilAttachment = &depthattref;

    VkSubpassDependency dep {
        .srcSubpass=VK_SUBPASS_EXTERNAL,
        .dstSubpass=0,
        .srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask=0,
        .dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dependencyFlags=0
    };

    auto vibdesc = VkVertexInputBindingDescription{
        0,
        3 * 4 + 3 * 4 + 2 * 4,
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkDescriptorBindingFlags dbf[2] = {0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
    auto dslbfc = make_vulkan_structure<VkDescriptorSetLayoutBindingFlagsCreateInfo>({
        .bindingCount=2,
        .pBindingFlags=dbf
    });

    pbtemplate = VkPipelineBuilder()
        .add_shader_stage(std::move(vshstc))
        .add_shader_stage(std::move(fshstc))
        .add_attachment(std::move(coloratt))
        .add_attachment(std::move(depthatt))
        .add_subpass(std::move(subpass))
        .add_subpass_dependency(std::move(dep))
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .set_fixed_scissor_count(1)
        .set_fixed_viewport_count(1)
        .primitive_restart_enable(false)
        .new_descriptor_set_layout(0, &dslbfc)
        .add_descriptor_set_binding(VkDescriptorSetLayoutBinding{
            .binding=0,
            .descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount=1,
            .stageFlags=VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers=nullptr
        })
        .add_descriptor_set_binding(VkDescriptorSetLayoutBinding{
            .binding=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            //INTEL XE HAS A maxPerStageDescriptorSamplers OF 64 AND maxPerStageDescriptorSampledImages OF 200, WTF???
            .descriptorCount=64,
            .stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers=nullptr
        })
        .depth_clamp_enable(false)
        .rasterizer_discard_enable(false)
        .line_width(1.)
        .depth_bias_enable(false)
        .depth_bounds_test_enable(false)
        .stencil_test_enable(false)
        .blending_logic_op_enable(false)
        .add_push_constant_range(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 48});
    current_pipelinest = CKVkPipelineState();
    current_pipelinest.set_vertex_format(CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX1);
    root_pipeline = CKVkPipelineState::build_pipeline(&pbtemplate, vkdev, current_pipelinest);
    pbtemplate.existing_render_pass(root_pipeline->render_pass());
    pbtemplate.existing_pipeline_layout(root_pipeline->pipeline_layout());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        CKVkBuffer *b = new CKVkBuffer(this);
        auto sz = sizeof(CKVkMatrixUniform) * 4096;
        b->create(sz, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void *d = b->map(0, sz);
        matubos.emplace_back(b, d);
    }

    depthim = create_memory_image(vkdev, vkphydev, swchiext.width, swchiext.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthv = create_image_view(vkdev, depthim.im, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    swchfb.resize(swchivw.size());
    for (size_t i = 0; i < swchfb.size(); ++i)
    {
        const VkImageView attachments[] = {swchivw[i], depthv};
        auto fbc = make_vulkan_structure<VkFramebufferCreateInfo>({
            .renderPass=root_pipeline->render_pass(),
            .attachmentCount=2,
            .pAttachments=attachments,
            .width=swchiext.width,
            .height=swchiext.height,
            .layers=1
        });
        if (VK_SUCCESS != vkCreateFramebuffer(vkdev, &fbc, nullptr, &swchfb[i]))
            return FALSE;
    }

    smplr = CKVkSamplerBuilder().build(vkdev);

    auto cmdpc = make_vulkan_structure<VkCommandPoolCreateInfo>({
        .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex=qfidx[0]
    });
    if (VK_SUCCESS != vkCreateCommandPool(vkdev, &cmdpc, nullptr, &cmdpool))
        return FALSE;

    cmdbuf.resize(MAX_FRAMES_IN_FLIGHT);
    auto cmdbufac = make_vulkan_structure<VkCommandBufferAllocateInfo>({
        .commandPool=cmdpool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=MAX_FRAMES_IN_FLIGHT
    });
    if (VK_SUCCESS != vkAllocateCommandBuffers(vkdev, &cmdbufac, cmdbuf.data()))
        return FALSE;

    auto matpsz = std::vector<VkDescriptorPoolSize>({{
        .type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount=MAX_FRAMES_IN_FLIGHT
    }, {
        .type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount=MAX_FRAMES_IN_FLIGHT * 64
    }});
    auto dpc = make_vulkan_structure<VkDescriptorPoolCreateInfo>({
        .maxSets=MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount=matpsz.size(),
        .pPoolSizes=matpsz.data()
    });
    if (VK_SUCCESS != vkCreateDescriptorPool(vkdev, &dpc, nullptr, &descpool))
        return FALSE;

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, root_pipeline->descriptor_set_layouts().front());
    uint32_t sz[] = {64, 64}; //both set needs 1024 varlen descriptors
    auto dsvdci = make_vulkan_structure<VkDescriptorSetVariableDescriptorCountAllocateInfo>({
        .descriptorSetCount=2,
        .pDescriptorCounts=sz
    });
    auto dsai = make_vulkan_structure<VkDescriptorSetAllocateInfo>({
        .pNext=&dsvdci,
        .descriptorPool=descpool,
        .descriptorSetCount=layouts.size(),
        .pSetLayouts=layouts.data()
    });
    descsets.resize(layouts.size());
    if (VK_SUCCESS != vkAllocateDescriptorSets(vkdev, &dsai, descsets.data()))
        return FALSE;
    update_descriptor_sets(true);

    auto semc = make_vulkan_structure<VkSemaphoreCreateInfo>({});
    auto fenc = make_vulkan_structure<VkFenceCreateInfo>({.flags=VK_FENCE_CREATE_SIGNALED_BIT});

    sem_image_available.resize(MAX_FRAMES_IN_FLIGHT);
    sem_render_finished.resize(MAX_FRAMES_IN_FLIGHT);
    fence_cmdbuf_exec.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        if (VK_SUCCESS != vkCreateSemaphore(vkdev, &semc, nullptr, &sem_image_available[i]) ||
            VK_SUCCESS != vkCreateSemaphore(vkdev, &semc, nullptr, &sem_render_finished[i]) ||
            VK_SUCCESS != vkCreateFence(vkdev, &fenc, nullptr, &fence_cmdbuf_exec[i]))
            return FALSE;

    m_ViewportData = CKViewportData {0, 0, (int)swchiext.width, (int)swchiext.height, 0., 1.};

    buf_deletion_queue = std::vector<std::vector<std::variant<CKVkVertexBuffer*, CKVkIndexBuffer*>>>(MAX_FRAMES_IN_FLIGHT);
    dynibo.resize(DYNAMIC_IBO_COUNT * MAX_FRAMES_IN_FLIGHT);
    unbuffered_vertex_draws = 0;
    unbuffered_index_draws = 0;

    return TRUE;
}

CKBOOL CKVkRasterizerContext::Resize(int PosX, int PosY, int Width, int Height, CKDWORD Flags)
{
    return TRUE;
}

CKBOOL CKVkRasterizerContext::Clear(CKDWORD Flags, CKDWORD Ccol, float Z, CKDWORD Stencil, int RectCount, CKRECT *rects)
{
    return TRUE;
}

CKBOOL CKVkRasterizerContext::BackToFront(CKBOOL vsync)
{
    return TRUE;
}

CKBOOL CKVkRasterizerContext::BeginScene()
{
    FrameMark;
    if (in_scene)
    {
        fprintf(stderr, "BeginScene() called while rendering is already active.\n");
        EndScene(); //try to recover??
    }
    vkWaitForFences(vkdev, 1, &fence_cmdbuf_exec[curfrm], VK_TRUE, ~0ULL);
    vkResetFences(vkdev, 1, &fence_cmdbuf_exec[curfrm]);
    if (!buf_deletion_queue[curfrm].empty())
    {
        fprintf(stderr, "deleting %lu buffers\n", buf_deletion_queue[curfrm].size());
        for (auto& b : buf_deletion_queue[curfrm])
        {
            std::visit(overloaded {
                [](CKVkVertexBuffer *b) { delete b; },
                [](CKVkIndexBuffer *b) { delete b; }
            }, b);
        }
        buf_deletion_queue[curfrm].clear();
    }
    if (VK_SUCCESS != vkAcquireNextImageKHR(vkdev, vkswch, ~0ULL, sem_image_available[curfrm], VK_NULL_HANDLE, &image_index))
    {
        fprintf(stderr, "AcquireNextImage failed\n");
        return FALSE;
    }
    if (VK_SUCCESS != vkResetCommandBuffer(cmdbuf[curfrm], 0))
    {
        fprintf(stderr, "ResetCommandBuffer failed\n");
        return FALSE;
    }
    auto cmdbufi = make_vulkan_structure<VkCommandBufferBeginInfo>({
        .flags=0,
        .pInheritanceInfo=nullptr
    });
    if (VK_SUCCESS != vkBeginCommandBuffer(cmdbuf[curfrm], &cmdbufi))
        return FALSE;

    auto rpi = make_vulkan_structure<VkRenderPassBeginInfo>({
        .renderPass=root_pipeline->render_pass(),
        .framebuffer=swchfb[image_index],
        .renderArea={.offset={0, 0}, .extent=swchiext}
    });
    VkClearValue colorc = {{{0., 0., 0., 1.}}};
    VkClearValue depthsc = {1., 0.};
    const VkClearValue clrv[] = {colorc, depthsc};
    rpi.clearValueCount = 2;
    rpi.pClearValues = clrv;

    in_scene = true;
    ubo_offset = 0;
    bound_pipeline = nullptr;

    directbat = 0;
    vbbat = 0;
    vbibbat = 0;

    vkCmdBeginRenderPass(cmdbuf[curfrm], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    //pl->command_bind_pipeline(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS);
    root_pipeline->command_bind_descriptor_sets(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &descsets[curfrm], 1, &ubo_offset);

    auto vpd = &m_ViewportData;
    VkViewport vp {
            (float)vpd->ViewX, (float)(vpd->ViewY + vpd->ViewHeight),
            (float)vpd->ViewWidth, (float)-vpd->ViewHeight,
            vpd->ViewZMin, vpd->ViewZMax
        };
    vkCmdSetViewport(cmdbuf[curfrm], 0, 1, &vp);
    VkRect2D sc{{0, 0}, swchiext};
    vkCmdSetScissor(cmdbuf[curfrm], 0, 1, &sc);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::EndScene()
{
    if (!in_scene)
    {
        fprintf(stderr, "EndScene() called while there's no scene being rendered.\n");
        return TRUE;
    }
    vkCmdEndRenderPass(cmdbuf[curfrm]);
    if (VK_SUCCESS != vkEndCommandBuffer(cmdbuf[curfrm]))
    {
        fprintf(stderr, "EndCommandBuffer failed\n");
        return FALSE;
    }

    VkSemaphore waitsem[] = {sem_image_available[curfrm]};
    VkPipelineStageFlags waitst[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    auto submiti = make_vulkan_structure<VkSubmitInfo>({
        .waitSemaphoreCount=1,
        .pWaitSemaphores=waitsem,
        .pWaitDstStageMask=waitst,
        .commandBufferCount=1,
        .pCommandBuffers=&cmdbuf[curfrm]
    });

    VkSemaphore signalsem[] = {sem_render_finished[curfrm]};
    submiti.signalSemaphoreCount = 1;
    submiti.pSignalSemaphores = signalsem;

    fprintf(stderr, "Submit\n");
    auto ret = vkQueueSubmit(gfxq, 1, &submiti, fence_cmdbuf_exec[curfrm]);
    if (VK_SUCCESS != ret)
    {
        fprintf(stderr, "QueueSubmit failed: %d\n", ret);
        return FALSE;
    }

    VkSwapchainKHR swch[] = {vkswch};
    auto pri = make_vulkan_structure<VkPresentInfoKHR>({
        .waitSemaphoreCount=1,
        .pWaitSemaphores=signalsem,
        .swapchainCount=1,
        .pSwapchains=swch,
        .pImageIndices=&image_index
    });

    if (VK_SUCCESS != vkQueuePresentKHR(prsq, &pri))
    {
        fprintf(stderr, "QueuePresent failed\n");
    }

    if (textures_updated)
    {
        update_descriptor_sets(false);
        textures_updated = false;
    }

    in_scene = false;
    ++curfrm;
    if (curfrm >= MAX_FRAMES_IN_FLIGHT)
        curfrm = 0;
    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetLight(CKDWORD Light, CKLightData *data)
{
    ZoneScopedN(__FUNCTION__);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::EnableLight(CKDWORD Light, CKBOOL Enable)
{
    ZoneScopedN(__FUNCTION__);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetMaterial(CKMaterialData *mat)
{
    ZoneScopedN(__FUNCTION__);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetViewport(CKViewportData *data)
{
    ZoneScopedN(__FUNCTION__);

    memcpy(&m_ViewportData, data, sizeof(m_ViewportData));
    if (in_scene)
    {
        VkViewport vp {
            (float)data->ViewX, (float)(data->ViewY + data->ViewHeight),
            (float)data->ViewWidth, (float)-data->ViewHeight,
            data->ViewZMin, data->ViewZMax
        };
        vkCmdSetViewport(cmdbuf[curfrm], 0, 1, &vp);
    }
    matrices.vp2d = VxMatrix::Identity();
    float (*m)[4] = (float(*)[4])&matrices.vp2d;
    m[0][0] = 2. / data->ViewWidth;
    m[1][1] = 2. / data->ViewHeight;
    m[2][2] = 0;
    m[3][0] = -(-2. * data->ViewX + data->ViewWidth) / data->ViewWidth;
    m[3][1] =  (-2. * data->ViewY + data->ViewHeight) / data->ViewHeight;

    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetTransformMatrix(VXMATRIX_TYPE Type, const VxMatrix &Mat)
{
    ZoneScopedN(__FUNCTION__);
    CKRasterizerContext::SetTransformMatrix(Type, Mat);
    CKDWORD UnityMatrixMask = 0;
    switch (Type)
    {
        case VXMATRIX_WORLD: UnityMatrixMask = WORLD_TRANSFORM; break;
        case VXMATRIX_VIEW: UnityMatrixMask = VIEW_TRANSFORM; break;
        case VXMATRIX_PROJECTION: UnityMatrixMask = PROJ_TRANSFORM; break;
        case VXMATRIX_TEXTURE0:
        case VXMATRIX_TEXTURE1:
        case VXMATRIX_TEXTURE2:
        case VXMATRIX_TEXTURE3:
        case VXMATRIX_TEXTURE4:
        case VXMATRIX_TEXTURE5:
        case VXMATRIX_TEXTURE6:
        case VXMATRIX_TEXTURE7:
            UnityMatrixMask = TEXTURE0_TRANSFORM << (Type - TEXTURE1_TRANSFORM);
            break;
    }
    if (VxMatrix::Identity() == Mat)
    {
        if ((m_UnityMatrixMask & UnityMatrixMask) != 0)
            return TRUE;
        m_UnityMatrixMask |= UnityMatrixMask;
    } else
        m_UnityMatrixMask &= ~UnityMatrixMask;

    switch (Type)
    {
        case VXMATRIX_WORLD:
        {
            m_WorldMatrix = matrices.world = Mat;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            //m_prgm->stage_uniform("world", CKGLUniformValue::make_f32mat4(1, (float*)&m_WorldMatrix));
            //VxMatrix tmat;
            //Vx3DTransposeMatrix(tmat, Mat); //row-major to column-major conversion madness
            //m_tiworldmtx = inv(tmat);
            //m_prgm->stage_uniform("tiworld", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldmtx));
            //Vx3DTransposeMatrix(tmat, m_ModelViewMatrix);
            //m_tiworldviewmtx = inv(tmat);
            //m_prgm->stage_uniform("tiworldview", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldviewmtx));
            m_MatrixUptodate &= ~0U ^ WORLD_TRANSFORM;
            break;
        }
        case VXMATRIX_VIEW:
        {
            m_ViewMatrix = matrices.view = Mat;
            Vx3DMultiplyMatrix(m_ModelViewMatrix, m_ViewMatrix, m_WorldMatrix);
            //m_prgm->stage_uniform("view", CKGLUniformValue::make_f32mat4(1, (float*)&m_ViewMatrix));
            m_MatrixUptodate = 0;
            //VxMatrix tmat;
            //Vx3DInverseMatrix(tmat, Mat);
            //m_viewpos = VxVector(tmat[3][0], tmat[3][1], tmat[3][2]);
            //m_prgm->stage_uniform("vpos", CKGLUniformValue::make_f32v3v(1, (float*)&m_viewpos));
            //Vx3DTransposeMatrix(tmat, m_ModelViewMatrix);
            //m_tiworldviewmtx = inv(tmat);
            //m_prgm->stage_uniform("tiworldview", CKGLUniformValue::make_f32mat4(1, (float*)&m_tiworldviewmtx));
            break;
        }
        case VXMATRIX_PROJECTION:
        {
            m_ProjectionMatrix = matrices.proj = Mat;
            //m_prgm->stage_uniform("proj", CKGLUniformValue::make_f32mat4(1, (float*)&m_ProjectionMatrix));
            //float (*m)[4] = (float(*)[4])&Mat;
            //float A = m[2][2];
            //float B = m[3][2];
            //float zp[2] = {-B / A, B / (1 - A)}; //for eye-distance fog calculation
            //m_prgm->stage_uniform("depth_range", CKGLUniformValue::make_f32v2v(1, zp, true));
            m_MatrixUptodate = 0;
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
            CKDWORD tex = Type - VXMATRIX_TEXTURE0;
            //m_textrmtx[tex] = Mat;
            //m_prgm->stage_uniform("textr[" + std::to_string(tex) + "]", CKGLUniformValue::make_f32mat4(1, (float*)&m_textrmtx[tex]));
            break;
        }
        default:
            return FALSE;
    }
    //if (in_scene)
    //    pl->command_push_constants(cmdbuf[curfrm], VK_SHADER_STAGE_VERTEX_BIT, 0, 192, &m_WorldMatrix);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetRenderState(VXRENDERSTATETYPE State, CKDWORD Value)
{
    ZoneScopedN(__FUNCTION__);
    if (m_StateCache[State].Value != Value || !m_StateCache[State].Valid)
    {
        ++ m_RenderStateCacheMiss;
        m_StateCache[State].Valid = 1;
        m_StateCache[State].Value = Value;
        return set_render_state_impl(State, Value);
    } else ++m_RenderStateCacheHit;
    return TRUE;
}

CKBOOL CKVkRasterizerContext::set_render_state_impl(VXRENDERSTATETYPE state, CKDWORD value)
{
    switch (state)
    {
        case VXRENDERSTATE_ZENABLE:
            //TODO: Check correctness
            //Reason: This is different from what we're doing in the GLRasterizer
            current_pipelinest.set_depth_test(value);
            return TRUE;
        case VXRENDERSTATE_ZWRITEENABLE:
            current_pipelinest.set_depth_write(value);
            return TRUE;
        case VXRENDERSTATE_ZFUNC:
            current_pipelinest.set_depth_func(static_cast<VXCMPFUNC>(value));
            return TRUE;
        case VXRENDERSTATE_ALPHABLENDENABLE:
            current_pipelinest.set_blending_enable(value);
            return TRUE;
        case VXRENDERSTATE_SRCBLEND:
            current_pipelinest.set_src_blend(static_cast<VXBLEND_MODE>(value), static_cast<VXBLEND_MODE>(value));
            return TRUE;
        case VXRENDERSTATE_DESTBLEND:
            current_pipelinest.set_dst_blend(static_cast<VXBLEND_MODE>(value), static_cast<VXBLEND_MODE>(value));
            return TRUE;
        case VXRENDERSTATE_INVERSEWINDING:
            current_pipelinest.set_inverse_winding(value);
            return TRUE;
        case VXRENDERSTATE_CULLMODE:
            current_pipelinest.set_cull_mode(static_cast<VXCULL>(value));
            return TRUE;
        case VXRENDERSTATE_FILLMODE:
            current_pipelinest.set_fill_mode(static_cast<VXFILL_MODE>(value));
            return TRUE;
    }
    return FALSE;
}

CKBOOL CKVkRasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}

CKBOOL CKVkRasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
    if (Stage != 0) return TRUE;
    int d[8] = {0};
    if (!Texture) d[0] = -1;
    else if (texture_binding.find(Texture) != texture_binding.end())
        d[0] = texture_binding[Texture];
    //!!FIXME: semantically confusing -- only relies on the pipeline layout which is shared across all pipeline objects
    if (in_scene)
        root_pipeline->command_push_constants(cmdbuf[curfrm], VK_SHADER_STAGE_VERTEX_BIT, 16, 32, d);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::SetTextureStageState(int Stage, CKRST_TEXTURESTAGESTATETYPE Tss, CKDWORD Value)
{
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKVkRasterizerContext::SetVertexShader(CKDWORD VShaderIndex)
{
    return CKRasterizerContext::SetVertexShader(VShaderIndex);
}

CKBOOL CKVkRasterizerContext::SetPixelShader(CKDWORD PShaderIndex)
{
    return CKRasterizerContext::SetPixelShader(PShaderIndex);
}

CKBOOL CKVkRasterizerContext::SetVertexShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetVertexShaderConstant(Register, Data, CstCount);
}

CKBOOL CKVkRasterizerContext::SetPixelShaderConstant(CKDWORD Register, const void *Data, CKDWORD CstCount)
{
    return CKRasterizerContext::SetPixelShaderConstant(Register, Data, CstCount);
}

CKBOOL CKVkRasterizerContext::DrawPrimitive(VXPRIMITIVETYPE pType, CKWORD *indices, int indexcount,
    VxDrawPrimitiveData *data)
{
#if LOG_DRAWPRIMITIVE
    fprintf(stderr, "drawprimitive ibp %p ic %d vc %d\n", indices, indexcount, data->VertexCount);
#endif
    ++directbat;
    ZoneScopedN(__FUNCTION__);

    CKDWORD vertexSize;
    CKDWORD vertexFormat = CKRSTGetVertexFormat((CKRST_DPFLAGS)data->Flags, vertexSize);

    //if ((data->Flags & CKRST_DP_DOCLIP)) ...

    CKVkVertexBuffer *vb = nullptr;
    auto vboid = std::make_pair(vertexFormat, DWORD(unbuffered_vertex_draws + curfrm * DYNAMIC_VBO_COUNT));
    if (++unbuffered_vertex_draws > DYNAMIC_VBO_COUNT) unbuffered_vertex_draws = 0;
    if (dynvbo.find(vboid) == dynvbo.end() ||
        dynvbo[vboid]->m_MaxVertexCount < data->VertexCount)
    {
        if (dynvbo[vboid])
            delete dynvbo[vboid];
        CKVertexBufferDesc vbd;
        vbd.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
        vbd.m_VertexFormat = vertexFormat;
        vbd.m_VertexSize = vertexSize;
        vbd.m_MaxVertexCount = (data->VertexCount + 100 > DEFAULT_VB_SIZE) ? data->VertexCount + 100 : DEFAULT_VB_SIZE;
        CKVkVertexBuffer *vb = new CKVkVertexBuffer(&vbd, this);
        vb->create();
        dynvbo[vboid] = vb;
    }
    vb = dynvbo[vboid];
    void *pbData = nullptr;
    CKDWORD vbase = 0;
    if (vb->m_CurrentVCount + data->VertexCount < vb->m_MaxVertexCount)
    {
        pbData = vb->lock(vertexSize * vb->m_CurrentVCount,
                          vertexSize * data->VertexCount);
        vbase = vb->m_CurrentVCount;
        vb->m_CurrentVCount += data->VertexCount;
    }
    else
    {
        pbData = vb->lock(0, vertexSize * data->VertexCount);
        vb->m_CurrentVCount = data->VertexCount;
    }
#if LOG_DRAWPRIMITIVE
    fprintf(stderr, "vbase %u current vc %u max vc %u\n", vbase, vb->m_CurrentVCount, vb->m_MaxVertexCount);
#endif
    {
        ZoneScopedN("CKRSTLoadVertexBuffer");
        CKRSTLoadVertexBuffer(static_cast<CKBYTE *>(pbData), vertexFormat, vertexSize, data);
    }
    vb->unlock();
    return draw_primitive_unbuffered_index_impl(pType, vb, vbase, data->VertexCount, indices, indexcount);
}

CKBOOL CKVkRasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartVertex,
    CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if LOG_DRAWPRIMITIVEVB
    fprintf(stderr, "drawprimitive vb %d %d\n", VertexCount, indexcount);
#endif
    ++vbbat;
    ZoneScopedN(__FUNCTION__);

    if (VertexBuffer >= m_VertexBuffers.Size()) return FALSE;
    CKVkVertexBuffer *vb = static_cast<CKVkVertexBuffer*>(m_VertexBuffers[VertexBuffer]);
    if (!vb) return FALSE;

    return draw_primitive_unbuffered_index_impl(pType, vb, StartVertex, VertexCount, indices, indexcount);
}

CKBOOL CKVkRasterizerContext::DrawPrimitiveVBIB(VXPRIMITIVETYPE pType, CKDWORD VB, CKDWORD IB, CKDWORD MinVIndex,
    CKDWORD VertexCount, CKDWORD StartIndex, int Indexcount)
{
#if LOG_DRAWPRIMITIVEVBIB
    fprintf(stderr, "drawprimitive vbib %d %d\n", VertexCount, Indexcount);
#endif
    ++vbibbat;
    ZoneScopedN(__FUNCTION__);

    if (VB >= m_VertexBuffers.Size()) return FALSE;
    CKVkVertexBuffer *vb = static_cast<CKVkVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vb) return FALSE;

    if (IB >= m_IndexBuffers.Size()) return FALSE;
    CKVkIndexBuffer *ib = static_cast<CKVkIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ib) return FALSE;

    return draw_primitive_impl(pType, vb, ib, VertexCount, Indexcount, 1, StartIndex, MinVIndex, 0);
}

bool CKVkRasterizerContext::draw_primitive_unbuffered_index_impl(VXPRIMITIVETYPE pType, CKVkVertexBuffer *vb,
                                                                 CKDWORD StartVertex, CKDWORD VertexCount,
                                                                 CKWORD *Indices, int IndexCount)
{
    int ibbase = 0;
    CKVkIndexBuffer *ibo = nullptr;
    if (Indices)
    {
        void *pdata = nullptr;
        auto iboid = unbuffered_index_draws + curfrm * DYNAMIC_IBO_COUNT;
        if (++unbuffered_index_draws >= DYNAMIC_IBO_COUNT) unbuffered_index_draws = 0;
        if (!dynibo[iboid] || dynibo[iboid]->m_MaxIndexCount < IndexCount)
        {
            if (dynibo[iboid])
                delete dynibo[iboid];
            CKIndexBufferDesc ibd;
            ibd.m_Flags = CKRST_VB_WRITEONLY | CKRST_VB_DYNAMIC;
            ibd.m_MaxIndexCount = IndexCount + 100 < DEFAULT_VB_SIZE ? DEFAULT_VB_SIZE : IndexCount + 100;
            ibd.m_CurrentICount = 0;
            CKVkIndexBuffer *ib = new CKVkIndexBuffer(&ibd, this);
            ib->create();
            dynibo[iboid] = ib;
        }
        ibo = dynibo[iboid];
        if (IndexCount + ibo->m_CurrentICount <= ibo->m_MaxIndexCount)
        {
            pdata = ibo->lock(2 * ibo->m_CurrentICount, 2 * IndexCount);
            ibbase = ibo->m_CurrentICount;
            ibo->m_CurrentICount += IndexCount;
        } else
        {
            pdata = ibo->lock(0, 2 * IndexCount);
            ibo->m_CurrentICount = IndexCount;
        }
        if (pdata)
            memcpy(pdata, Indices, 2 * IndexCount);
        ibo->unlock();
    }
#if LOG_DRAWPRIMITIVEVB
    fprintf(stderr, "ibbase %d cur ic %u max ic %u\n", ibbase, ibo->m_CurrentICount, ibo->m_MaxIndexCount);
#endif

    draw_primitive_impl(pType, vb, ibo, VertexCount, IndexCount | 0x80000000, 1, ibbase, StartVertex, 0);

    return TRUE;
}

bool CKVkRasterizerContext::draw_primitive_impl(VXPRIMITIVETYPE pty, CKVkVertexBuffer *vb, CKVkIndexBuffer *ib, uint32_t vtxcnt,
                                                uint32_t idxcnt, uint32_t instcnt, uint32_t firstidx, int32_t vtxoffset,
                                                uint32_t firstinst)
{
    current_pipelinest.set_primitive_type(pty);
    current_pipelinest.set_vertex_format(vb->m_VertexFormat);
    bind_pipeline();

    vb->bind(cmdbuf[curfrm]);
    if (ib)
        ib->bind(cmdbuf[curfrm]);

    uint8_t *dest = static_cast<uint8_t*>(matubos[curfrm].second);
    dest = dest + ubo_offset;
    memcpy(dest, &matrices, sizeof(CKVkMatrixUniform));
    //!!FIXME (dumb design): only pipeline layout is used in the following call, which is shared across all pipelines
    bound_pipeline->command_bind_descriptor_sets(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &descsets[curfrm], 1, &ubo_offset);
    int flags[4] = {0};
    if (vb->m_VertexFormat & CKRST_VF_RASTERPOS)
        flags[0] = 1;
    bound_pipeline->command_push_constants(cmdbuf[curfrm], VK_SHADER_STAGE_VERTEX_BIT, 0, 16, flags);

    //if (!(idxcnt & 0x80000000)){
    if (ib){
    if (!(idxcnt & 0x80000000))
        vkCmdDrawIndexed(cmdbuf[curfrm], idxcnt, instcnt, firstidx, vtxoffset, firstinst);
        }
    else
        vkCmdDraw(cmdbuf[curfrm], vtxcnt, 1, vtxoffset, 0);//}
    ubo_offset += sizeof(CKVkMatrixUniform);
    return TRUE;
}

CKBOOL CKVkRasterizerContext::CreateObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type, void *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    int result;

    if (ObjIndex >= m_Textures.Size())
        return 0;
    switch (Type)
    {
        case CKRST_OBJ_TEXTURE:
            result = CreateTexture(ObjIndex, static_cast<CKTextureDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_SPRITE:
        {
            return 0;
            result = CreateSprite(ObjIndex, static_cast<CKSpriteDesc *>(DesiredFormat));
            CKSpriteDesc* desc = m_Sprites[ObjIndex];
            fprintf(stderr, "idx: %d\n", ObjIndex);
            for (auto it = desc->Textures.Begin(); it != desc->Textures.End(); ++it)
            {
                fprintf(stderr, "(%d,%d) WxH: %dx%d, SWxSH: %dx%d\n", it->x, it->y, it->w, it->h, it->sw, it->sh);
            }
            fprintf(stderr, "---\n");
            break;
        }
        case CKRST_OBJ_VERTEXBUFFER:
            result = CreateVertexBuffer(ObjIndex, static_cast<CKVertexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_INDEXBUFFER:
            result = CreateIndexBuffer(ObjIndex, static_cast<CKIndexBufferDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_VERTEXSHADER:
            result =
                CreateVertexShader(ObjIndex, static_cast<CKVertexShaderDesc *>(DesiredFormat));
            break;
        case CKRST_OBJ_PIXELSHADER:
            result =
                CreatePixelShader(ObjIndex, static_cast<CKPixelShaderDesc *>(DesiredFormat));
            break;
        default:
            return 0;
    }
    return result;
}

CKBOOL CKVkRasterizerContext::DeleteObject(CKDWORD ObjIndex, CKRST_OBJECTTYPE Type)
{
    if (Type == CKRST_OBJ_TEXTURE)
    {
        texture_binding.erase(ObjIndex);
        textures_updated = true;
    }
    if (Type & (CKRST_OBJ_VERTEXBUFFER | CKRST_OBJ_INDEXBUFFER))
    {
        if (in_scene)
            fprintf(stderr, "warning: deleting drawing buffer %d while cmdbuf is being recorded\n", ObjIndex);
        else
        {
            fprintf(stderr, "scheduling deletion of drawing buffer %d\n", ObjIndex);
            uint32_t lastfrm = (curfrm == 0) ? MAX_FRAMES_IN_FLIGHT - 1 : curfrm - 1;
            if (Type & CKRST_OBJ_VERTEXBUFFER)
            {
                buf_deletion_queue[lastfrm].emplace_back(static_cast<CKVkVertexBuffer*>(m_VertexBuffers[ObjIndex]));
                m_VertexBuffers[ObjIndex] = NULL;
            }
            else
            {
                buf_deletion_queue[lastfrm].emplace_back(static_cast<CKVkIndexBuffer*>(m_IndexBuffers[ObjIndex]));
                m_IndexBuffers[ObjIndex] = NULL;
            }
            return TRUE;
        }
    }
    return CKRasterizerContext::DeleteObject(ObjIndex, Type);
}

void * CKVkRasterizerContext::LockVertexBuffer(CKDWORD VB, CKDWORD StartVertex, CKDWORD VertexCount,
    CKRST_LOCKFLAGS Lock)
{
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size()) return NULL;
    CKVkVertexBuffer *vb = static_cast<CKVkVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vb) return NULL;
    return vb->lock(StartVertex * vb->m_VertexSize, VertexCount * vb->m_VertexSize);
}

CKBOOL CKVkRasterizerContext::UnlockVertexBuffer(CKDWORD VB)
{
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size()) return FALSE;
    CKVkVertexBuffer *vb = static_cast<CKVkVertexBuffer*>(m_VertexBuffers[VB]);
    if (!vb) return FALSE;
    vb->unlock();
    return TRUE;
}

CKBOOL CKVkRasterizerContext::LoadTexture(CKDWORD Texture, const VxImageDescEx &SurfDesc, int miplevel)
{
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    CKVkTexture *tex = static_cast<CKVkTexture*>(m_Textures[Texture]);
    if (!tex)
        return FALSE;
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
    if (!(SurfDesc.AlphaMask || SurfDesc.Flags >= _DXT1)) VxDoAlphaBlit(dst, 255);
    tex->load(dst.Image);
    delete dst.Image;
    return TRUE;
}

CKBOOL CKVkRasterizerContext::CopyToTexture(CKDWORD Texture, VxRect *Src, VxRect *Dest, CKRST_CUBEFACE Face)
{
    return CKRasterizerContext::CopyToTexture(Texture, Src, Dest, Face);
}

CKBOOL CKVkRasterizerContext::SetTargetTexture(CKDWORD TextureObject, int Width, int Height, CKRST_CUBEFACE Face,
    CKBOOL GenerateMipMap)
{
    return CKRasterizerContext::SetTargetTexture(TextureObject, Width, Height, Face, GenerateMipMap);
}

CKBOOL CKVkRasterizerContext::DrawSprite(CKDWORD Sprite, VxRect *src, VxRect *dst)
{
    return CKRasterizerContext::DrawSprite(Sprite, src, dst);
}

int CKVkRasterizerContext::CopyToMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyToMemoryBuffer(rect, buffer, img_desc);
}

int CKVkRasterizerContext::CopyFromMemoryBuffer(CKRECT *rect, VXBUFFER_TYPE buffer, const VxImageDescEx &img_desc)
{
    return CKRasterizerContext::CopyFromMemoryBuffer(rect, buffer, img_desc);
}

CKBOOL CKVkRasterizerContext::SetUserClipPlane(CKDWORD ClipPlaneIndex, const VxPlane &PlaneEquation)
{
    return CKRasterizerContext::SetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

CKBOOL CKVkRasterizerContext::GetUserClipPlane(CKDWORD ClipPlaneIndex, VxPlane &PlaneEquation)
{
    return CKRasterizerContext::GetUserClipPlane(ClipPlaneIndex, PlaneEquation);
}

void * CKVkRasterizerContext::LockIndexBuffer(CKDWORD IB, CKDWORD StartIndex, CKDWORD IndexCount, CKRST_LOCKFLAGS Lock)
{
    if (IB >= m_IndexBuffers.Size()) return NULL;
    CKVkIndexBuffer *ib = static_cast<CKVkIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ib) return NULL;
    return ib->lock(2 * StartIndex, 2 * IndexCount);
}

CKBOOL CKVkRasterizerContext::UnlockIndexBuffer(CKDWORD IB)
{
    if (IB >= m_IndexBuffers.Size()) return FALSE;
    CKVkIndexBuffer *ib = static_cast<CKVkIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ib) return FALSE;
    ib->unlock();
    return TRUE;
}

void CKVkRasterizerContext::set_title_status(const char *fmt, ...)
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
        ts = m_orig_title + " | " + ts;
    } else ts = m_orig_title;

    SetWindowTextA(GetAncestor((HWND)m_Window, GA_ROOT), ts.c_str());
}

CKBOOL CKVkRasterizerContext::CreateTexture(CKDWORD Texture, CKTextureDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (Texture >= m_Textures.Size())
        return FALSE;
    if (m_Textures[Texture])
        return TRUE;
#if LOG_CREATETEXTURE
    fprintf(stderr, "create texture %d %dx%d %x\n", Texture, DesiredFormat->Format.Width, DesiredFormat->Format.Height, DesiredFormat->Flags);
#endif

    auto tex = new CKVkTexture(this, DesiredFormat);
    tex->create();

    m_Textures[Texture] = tex;
    texture_binding[Texture] = 0;
    textures_updated = true;

    return TRUE;
}

CKBOOL CKVkRasterizerContext::CreateVertexShader(CKDWORD VShader, CKVertexShaderDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKVkRasterizerContext::CreatePixelShader(CKDWORD PShader, CKPixelShaderDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKVkRasterizerContext::CreateVertexBuffer(CKDWORD VB, CKVertexBufferDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (VB >= m_VertexBuffers.Size() || !DesiredFormat)
        return FALSE;
    if (m_VertexBuffers[VB])
        delete m_VertexBuffers[VB];

    CKVkVertexBuffer* vb = new CKVkVertexBuffer(DesiredFormat, this);
    vb->create();
    m_VertexBuffers[VB] = vb;
    return TRUE;
}

CKBOOL CKVkRasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return FALSE;
    if (m_IndexBuffers[IB])
        delete m_IndexBuffers[IB];

    CKVkIndexBuffer* ib = new CKVkIndexBuffer(DesiredFormat, this);
    ib->create();
    m_IndexBuffers[IB] = ib;
    return TRUE;
}

void CKVkRasterizerContext::FlushCaches()
{
}

void CKVkRasterizerContext::FlushNonManagedObjects()
{
}

void CKVkRasterizerContext::ReleaseStateBlocks()
{
}

void CKVkRasterizerContext::ReleaseBuffers()
{
    
}

void CKVkRasterizerContext::ClearStreamCache()
{
}

void CKVkRasterizerContext::ReleaseScreenBackup()
{}

void CKVkRasterizerContext::update_descriptor_sets(bool init)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkDescriptorBufferInfo bi {
            .buffer=matubos[i].first->get_buffer(),
            .offset=0,
            .range=sizeof(CKVkMatrixUniform)
        };
        std::vector<VkDescriptorImageInfo> iis;
        for (auto &t : texture_binding)
        {
            t.second = iis.size();
            iis.push_back({
                .sampler=smplr->sampler(),
                .imageView=static_cast<CKVkTexture*>(m_Textures[t.first])->view(),
                .imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
        }
        std::vector<VkWriteDescriptorSet> dsws;
        if (init) dsws.push_back(make_vulkan_structure<VkWriteDescriptorSet>({
            .dstSet=descsets[i],
            .dstBinding=0,
            .dstArrayElement=0,
            .descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo=&bi
        }));
        if (iis.size()) dsws.push_back(make_vulkan_structure<VkWriteDescriptorSet>({
            .dstSet=descsets[i],
            .dstBinding=1,
            .dstArrayElement=0,
            .descriptorCount=iis.size(),
            .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo=iis.data()
        }));
        if (dsws.size())
            vkUpdateDescriptorSets(vkdev, dsws.size(), dsws.data(), 0, nullptr);
    }
}

void CKVkRasterizerContext::bind_pipeline()
{
    if (pls.find(current_pipelinest) == pls.end())
    {
        auto pl = CKVkPipelineState::build_pipeline(&pbtemplate, vkdev, current_pipelinest);
        pls[current_pipelinest] = pl;
    }
    auto pl = pls[current_pipelinest];
    if (bound_pipeline != pl)
    {
        pl->command_bind_pipeline(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS);
        bound_pipeline = pl;
    }
}

VxMatrix inv(const VxMatrix &_m)
{
    ZoneScopedN(__FUNCTION__);
    //taken from https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    float (*m)[4] = (float(*)[4])&_m;
    float A2323 = m[2][2] * m[3][3] - m[2][3] * m[3][2] ;
    float A1323 = m[2][1] * m[3][3] - m[2][3] * m[3][1] ;
    float A1223 = m[2][1] * m[3][2] - m[2][2] * m[3][1] ;
    float A0323 = m[2][0] * m[3][3] - m[2][3] * m[3][0] ;
    float A0223 = m[2][0] * m[3][2] - m[2][2] * m[3][0] ;
    float A0123 = m[2][0] * m[3][1] - m[2][1] * m[3][0] ;
    float A2313 = m[1][2] * m[3][3] - m[1][3] * m[3][2] ;
    float A1313 = m[1][1] * m[3][3] - m[1][3] * m[3][1] ;
    float A1213 = m[1][1] * m[3][2] - m[1][2] * m[3][1] ;
    float A2312 = m[1][2] * m[2][3] - m[1][3] * m[2][2] ;
    float A1312 = m[1][1] * m[2][3] - m[1][3] * m[2][1] ;
    float A1212 = m[1][1] * m[2][2] - m[1][2] * m[2][1] ;
    float A0313 = m[1][0] * m[3][3] - m[1][3] * m[3][0] ;
    float A0213 = m[1][0] * m[3][2] - m[1][2] * m[3][0] ;
    float A0312 = m[1][0] * m[2][3] - m[1][3] * m[2][0] ;
    float A0212 = m[1][0] * m[2][2] - m[1][2] * m[2][0] ;
    float A0113 = m[1][0] * m[3][1] - m[1][1] * m[3][0] ;
    float A0112 = m[1][0] * m[2][1] - m[1][1] * m[2][0] ;

    float det = m[0][0] * ( m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223 )
        - m[0][1] * ( m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223 )
        + m[0][2] * ( m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123 )
        - m[0][3] * ( m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123 ) ;
    det = 1 / det;
    float ret[4][4];
    ret[0][0] = det *   ( m[1][1] * A2323 - m[1][2] * A1323 + m[1][3] * A1223 );
    ret[0][1] = det * - ( m[0][1] * A2323 - m[0][2] * A1323 + m[0][3] * A1223 );
    ret[0][2] = det *   ( m[0][1] * A2313 - m[0][2] * A1313 + m[0][3] * A1213 );
    ret[0][3] = det * - ( m[0][1] * A2312 - m[0][2] * A1312 + m[0][3] * A1212 );
    ret[1][0] = det * - ( m[1][0] * A2323 - m[1][2] * A0323 + m[1][3] * A0223 );
    ret[1][1] = det *   ( m[0][0] * A2323 - m[0][2] * A0323 + m[0][3] * A0223 );
    ret[1][2] = det * - ( m[0][0] * A2313 - m[0][2] * A0313 + m[0][3] * A0213 );
    ret[1][3] = det *   ( m[0][0] * A2312 - m[0][2] * A0312 + m[0][3] * A0212 );
    ret[2][0] = det *   ( m[1][0] * A1323 - m[1][1] * A0323 + m[1][3] * A0123 );
    ret[2][1] = det * - ( m[0][0] * A1323 - m[0][1] * A0323 + m[0][3] * A0123 );
    ret[2][2] = det *   ( m[0][0] * A1313 - m[0][1] * A0313 + m[0][3] * A0113 );
    ret[2][3] = det * - ( m[0][0] * A1312 - m[0][1] * A0312 + m[0][3] * A0112 );
    ret[3][0] = det * - ( m[1][0] * A1223 - m[1][1] * A0223 + m[1][2] * A0123 );
    ret[3][1] = det *   ( m[0][0] * A1223 - m[0][1] * A0223 + m[0][2] * A0123 );
    ret[3][2] = det * - ( m[0][0] * A1213 - m[0][1] * A0213 + m[0][2] * A0113 );
    ret[3][3] = det *   ( m[0][0] * A1212 - m[0][1] * A0212 + m[0][2] * A0112 );

    return VxMatrix(ret);
}
