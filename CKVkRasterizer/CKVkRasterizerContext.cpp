#include "CKVkRasterizer.h"
#include "CKVkBuffer.h"
#include "CKVkVertexBuffer.h"
#include "CKVkIndexBuffer.h"
#include "VkPipelineBuilder.h"
#include "ManagedVulkanPipeline.h"
#include "ResourceManagement.h"

#include <algorithm>

#define DYNAMIC_VBO_COUNT 64
#define DYNAMIC_IBO_COUNT 64

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
    for (auto &mu : matubos)
    {
        mu.first->unmap();
        delete mu.first;
    }
    for (auto &s : vksimgavail)
        vkDestroySemaphore(vkdev, s, nullptr);
    for (auto &s : vksrenderfinished)
        vkDestroySemaphore(vkdev, s, nullptr);
    for (auto &f : vkffrminfl)
        vkDestroyFence(vkdev, f, nullptr);
    vkDestroyCommandPool(vkdev, cmdpool, nullptr);
    vkDestroyDescriptorPool(vkdev, descpool, nullptr);
    vkDestroyImageView(vkdev, depthv, nullptr);
    destroy_memory_image(vkdev, depthim);
    for (auto &fb : swchfb)
        vkDestroyFramebuffer(vkdev, fb, nullptr);
    delete pl;
    vkDestroyShaderModule(vkdev, vsh, nullptr);
    vkDestroyShaderModule(vkdev, fsh, nullptr);
    for (auto &ivw : swchivw)
        vkDestroyImageView(vkdev, ivw, nullptr);
    vkDestroySwapchainKHR(vkdev, vkswch, nullptr);
    vkDestroySurfaceKHR(vkinst, vksurface, nullptr);

    m_DirtyRects.Clear();
    m_PixelShaders.Clear();
    m_VertexShaders.Clear();
    m_IndexBuffers.Clear();
    m_VertexBuffers.Clear();
    m_Sprites.Clear();
    m_Textures.Clear();
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

    VkPipelineColorBlendAttachmentState blendatt{
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
    };

    pl = VkPipelineBuilder()
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
        .add_input_binding(VkVertexInputBindingDescription{
            0,
            3 * 4 + 3 * 4 + 2 * 4,
            VK_VERTEX_INPUT_RATE_VERTEX
        })
        .add_vertex_attribute(VkVertexInputAttributeDescription{
            0, 0,
            VK_FORMAT_R32G32B32_SFLOAT, // POS
            0
        })
        .add_vertex_attribute(VkVertexInputAttributeDescription{
            1, 0,
            VK_FORMAT_R32G32B32_SFLOAT, // NORM
            12
        })
        .add_vertex_attribute(VkVertexInputAttributeDescription{
            2, 0,
            VK_FORMAT_R32G32_SFLOAT,    // TEX
            24
        })
        .primitive_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .primitive_restart_enable(false)
        .new_descriptor_set_layout(0)
        .add_descriptor_set_binding(VkDescriptorSetLayoutBinding{
            .binding=0,
            .descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount=1,
            .stageFlags=VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers=nullptr
        })
        .depth_clamp_enable(false)
        .rasterizer_discard_enable(false)
        .polygon_mode(VK_POLYGON_MODE_FILL)
        .line_width(1.)
        .cull_mode(VK_CULL_MODE_BACK_BIT)
        .front_face(VK_FRONT_FACE_CLOCKWISE)
        .depth_bias_enable(false)
        .depth_test_enable(true)
        .depth_write_enable(true)
        .depth_op(VK_COMPARE_OP_LESS)
        .depth_bounds_test_enable(false)
        .stencil_test_enable(false)
        .add_blending_attachment(std::move(blendatt))
        .blending_logic_op_enable(false)
        .add_push_constant_range(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 192})
        .build(vkdev);

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
            .renderPass=pl->render_pass(),
            .attachmentCount=2,
            .pAttachments=attachments,
            .width=swchiext.width,
            .height=swchiext.height,
            .layers=1
        });
        if (VK_SUCCESS != vkCreateFramebuffer(vkdev, &fbc, nullptr, &swchfb[i]))
            return FALSE;
    }

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

    auto matpsz = VkDescriptorPoolSize {
        .type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount=MAX_FRAMES_IN_FLIGHT
    };
    auto dpc = make_vulkan_structure<VkDescriptorPoolCreateInfo>({
        .maxSets=MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount=1,
        .pPoolSizes=&matpsz
    });
    if (VK_SUCCESS != vkCreateDescriptorPool(vkdev, &dpc, nullptr, &descpool))
        return FALSE;

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, pl->descriptor_set_layouts().front());
    auto dsai = make_vulkan_structure<VkDescriptorSetAllocateInfo>({
        .descriptorPool = descpool,
        .descriptorSetCount = layouts.size(),
        .pSetLayouts = layouts.data()
    });
    descsets.resize(layouts.size());
    if (VK_SUCCESS != vkAllocateDescriptorSets(vkdev, &dsai, descsets.data()))
        return FALSE;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkDescriptorBufferInfo bi {
            .buffer=matubos[i].first->get_buffer(),
            .offset=0,
            .range=sizeof(CKVkMatrixUniform)
        };
        auto dsw = make_vulkan_structure<VkWriteDescriptorSet>({
            .dstSet=descsets[i],
            .dstBinding=0,
            .dstArrayElement=0,
            .descriptorCount=1,
            .descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo=&bi
        });
        vkUpdateDescriptorSets(vkdev, 1, &dsw, 0, nullptr);
    }

    auto semc = make_vulkan_structure<VkSemaphoreCreateInfo>({});
    auto fenc = make_vulkan_structure<VkFenceCreateInfo>({.flags=VK_FENCE_CREATE_SIGNALED_BIT});

    vksimgavail.resize(MAX_FRAMES_IN_FLIGHT);
    vksrenderfinished.resize(MAX_FRAMES_IN_FLIGHT);
    vkffrminfl.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        if (VK_SUCCESS != vkCreateSemaphore(vkdev, &semc, nullptr, &vksimgavail[i]) ||
            VK_SUCCESS != vkCreateSemaphore(vkdev, &semc, nullptr, &vksrenderfinished[i]) ||
            VK_SUCCESS != vkCreateFence(vkdev, &fenc, nullptr, &vkffrminfl[i]))
            return FALSE;

    m_ViewportData = CKViewportData {0, 0, (int)swchiext.width, (int)swchiext.height, 0., 1.};

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
/*
    vkWaitForFences(vkdev, 1, &vkffrminfl[curfrm], VK_TRUE, ~0ULL);
    vkResetFences(vkdev, 1, &vkffrminfl[curfrm]);
    uint32_t imidx;
    vkAcquireNextImageKHR(vkdev, vkswch, ~0ULL, vksimgavail[curfrm], VK_NULL_HANDLE, &imidx);
    vkResetCommandBuffer(cmdbuf[curfrm], 0);
    auto cmdbufi = make_vulkan_structure<VkCommandBufferBeginInfo>();
    cmdbufi.flags = 0;
    cmdbufi.pInheritanceInfo = nullptr;
    if (VK_SUCCESS != vkBeginCommandBuffer(cmdbuf[curfrm], &cmdbufi))
        return FALSE;
    
    auto rpi = make_vulkan_structure<VkRenderPassBeginInfo>();
    rpi.renderPass = vkrp;
    rpi.framebuffer = swchfb[imidx];//
    rpi.renderArea.offset = {0, 0};
    rpi.renderArea.extent = swchiext;
    VkClearValue clearColor = {{{0., 0., 0., 1.}}};
    rpi.clearValueCount = 1;
    rpi.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmdbuf[curfrm], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS, vkpl);
    VkViewport vp{0., 1., (float)swchiext.width, (float)swchiext.height, 0., 1.};
    vkCmdSetViewport(cmdbuf[curfrm], 0, 1, &vp);
    VkRect2D sc{{0, 0}, swchiext};
    vkCmdSetScissor(cmdbuf[curfrm], 0, 1, &sc);
    vkCmdDraw(cmdbuf[curfrm], 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdbuf[curfrm]);
    if (VK_SUCCESS != vkEndCommandBuffer(cmdbuf[curfrm]))
        return FALSE;

    auto submiti = make_vulkan_structure<VkSubmitInfo>();

    VkSemaphore waitsem[] = {vksimgavail[curfrm]};
    VkPipelineStageFlags waitst[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submiti.waitSemaphoreCount = 1;
    submiti.pWaitSemaphores = waitsem;
    submiti.pWaitDstStageMask = waitst;
    submiti.commandBufferCount = 1;
    submiti.pCommandBuffers = &cmdbuf[curfrm];

    VkSemaphore signalsem[] = {vksrenderfinished[curfrm]};
    submiti.signalSemaphoreCount = 1;
    submiti.pSignalSemaphores = signalsem;

    if (VK_SUCCESS != vkQueueSubmit(gfxq, 1, &submiti, vkffrminfl[curfrm]))
        return FALSE;

    auto pri = make_vulkan_structure<VkPresentInfoKHR>();
    pri.waitSemaphoreCount = 1;
    pri.pWaitSemaphores = signalsem;
    VkSwapchainKHR swch[] = {vkswch};
    pri.swapchainCount = 1;
    pri.pSwapchains = swch;
    pri.pImageIndices = &imidx;

    vkQueuePresentKHR(prsq, &pri);
*/

    ++curfrm;
    if (curfrm >= MAX_FRAMES_IN_FLIGHT)
        curfrm = 0;
    return TRUE;
}

CKBOOL CKVkRasterizerContext::BeginScene()
{
    FrameMark;
    vkWaitForFences(vkdev, 1, &vkffrminfl[curfrm], VK_TRUE, ~0ULL);
    vkResetFences(vkdev, 1, &vkffrminfl[curfrm]);
    vkAcquireNextImageKHR(vkdev, vkswch, ~0ULL, vksimgavail[curfrm], VK_NULL_HANDLE, &image_index);
    vkResetCommandBuffer(cmdbuf[curfrm], 0);
    auto cmdbufi = make_vulkan_structure<VkCommandBufferBeginInfo>({
        .flags=0,
        .pInheritanceInfo=nullptr
    });
    if (VK_SUCCESS != vkBeginCommandBuffer(cmdbuf[curfrm], &cmdbufi))
        return FALSE;

    auto rpi = make_vulkan_structure<VkRenderPassBeginInfo>({
        .renderPass=pl->render_pass(),
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

    vkCmdBeginRenderPass(cmdbuf[curfrm], &rpi, VK_SUBPASS_CONTENTS_INLINE);
    pl->command_bind_pipeline(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS);
    pl->command_bind_descriptor_sets(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &descsets[curfrm], 1, &ubo_offset);

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
    vkCmdEndRenderPass(cmdbuf[curfrm]);
    if (VK_SUCCESS != vkEndCommandBuffer(cmdbuf[curfrm]))
        return FALSE;

    in_scene = false;


    VkSemaphore waitsem[] = {vksimgavail[curfrm]};
    VkPipelineStageFlags waitst[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    auto submiti = make_vulkan_structure<VkSubmitInfo>({
        .waitSemaphoreCount=1,
        .pWaitSemaphores=waitsem,
        .pWaitDstStageMask=waitst,
        .commandBufferCount=1,
        .pCommandBuffers=&cmdbuf[curfrm]
    });

    VkSemaphore signalsem[] = {vksrenderfinished[curfrm]};
    submiti.signalSemaphoreCount = 1;
    submiti.pSignalSemaphores = signalsem;

    if (VK_SUCCESS != vkQueueSubmit(gfxq, 1, &submiti, vkffrminfl[curfrm]))
        return FALSE;

    VkSwapchainKHR swch[] = {vkswch};
    auto pri = make_vulkan_structure<VkPresentInfoKHR>({
        .waitSemaphoreCount=1,
        .pWaitSemaphores=signalsem,
        .swapchainCount=1,
        .pSwapchains=swch,
        .pImageIndices=&image_index
    });

    vkQueuePresentKHR(prsq, &pri);
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
    return TRUE;
}

CKBOOL CKVkRasterizerContext::GetRenderState(VXRENDERSTATETYPE State, CKDWORD *Value)
{
    return CKRasterizerContext::GetRenderState(State, Value);
}

CKBOOL CKVkRasterizerContext::SetTexture(CKDWORD Texture, int Stage)
{
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
    fprintf(stderr, "drawprimitive ib %p %d\n", indices, indexcount);
#endif
    ++directbat;
    ZoneScopedN(__FUNCTION__);
    return FALSE;
}

CKBOOL CKVkRasterizerContext::DrawPrimitiveVB(VXPRIMITIVETYPE pType, CKDWORD VertexBuffer, CKDWORD StartIndex,
    CKDWORD VertexCount, CKWORD *indices, int indexcount)
{
#if LOG_DRAWPRIMITIVEVB
    fprintf(stderr, "drawprimitive vb %d %d\n", VertexCount, indexcount);
#endif
    ++vbbat;
    ZoneScopedN(__FUNCTION__);
    return FALSE;
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
    CKVkVertexBuffer *vbo = static_cast<CKVkVertexBuffer*>(m_VertexBuffers[VB]);
    if (vbo->m_VertexFormat != (CKRST_VF_POSITION | CKRST_VF_NORMAL | CKRST_VF_TEX1))
        return FALSE;
    if (!vbo) return FALSE;

    if (IB >= m_IndexBuffers.Size()) return FALSE;
    CKVkIndexBuffer *ibo = static_cast<CKVkIndexBuffer*>(m_IndexBuffers[IB]);
    if (!ibo) return FALSE;

    vbo->bind(cmdbuf[curfrm]);
    ibo->bind(cmdbuf[curfrm]);

    if (pType != VX_TRIANGLELIST)
        return FALSE;
    uint8_t *dest = static_cast<uint8_t*>(matubos[curfrm].second);
    dest = dest + ubo_offset;
    memcpy(dest, &matrices, sizeof(CKVkMatrixUniform));
    pl->command_bind_descriptor_sets(cmdbuf[curfrm], VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &descsets[curfrm], 1, &ubo_offset);
    vkCmdDrawIndexed(cmdbuf[curfrm], Indexcount, 1, StartIndex, MinVIndex, 0);
    ubo_offset += sizeof(CKVkMatrixUniform);
    return TRUE;
}

/*CKBOOL CKVkRasterizerContext::InternalDrawPrimitive(VXPRIMITIVETYPE pType, CKGLVertexBuffer *vbo, CKDWORD vbase, CKDWORD vcnt, WORD* idx, GLuint icnt, bool vbbound)
{
    ZoneScopedN(__FUNCTION__);
    return TRUE;
}*/

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

    return FALSE;
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
        return 0;
    if (m_VertexBuffers[VB])
        delete m_VertexBuffers[VB];

    CKVkVertexBuffer* vb = new CKVkVertexBuffer(DesiredFormat, this);
    vb->create();
    m_VertexBuffers[VB] = vb;
    return FALSE;
}

CKBOOL CKVkRasterizerContext::CreateIndexBuffer(CKDWORD IB, CKIndexBufferDesc *DesiredFormat)
{
    ZoneScopedN(__FUNCTION__);
    if (IB >= m_IndexBuffers.Size() || !DesiredFormat)
        return 0;
    if (m_IndexBuffers[IB])
        delete m_IndexBuffers[IB];

    CKVkIndexBuffer* ib = new CKVkIndexBuffer(DesiredFormat, this);
    ib->create();
    m_IndexBuffers[IB] = ib;
    return FALSE;
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
{
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
