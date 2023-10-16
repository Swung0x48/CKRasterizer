#include "CKVkPipelineBuilder.h"

CKVkPipelineBuilder::CKVkPipelineBuilder() :
    vkrp{}
{
    iasc = make_vulkan_structure<VkPipelineInputAssemblyStateCreateInfo>();
    rstc = make_vulkan_structure<VkPipelineRasterizationStateCreateInfo>();
    msstc = make_vulkan_structure<VkPipelineMultisampleStateCreateInfo>();
    msstc.sampleShadingEnable = VK_FALSE;
    msstc.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msstc.minSampleShading = 1.;
    msstc.pSampleMask = nullptr;
    msstc.alphaToCoverageEnable = VK_FALSE;
    msstc.alphaToOneEnable = VK_FALSE;
    dssc = make_vulkan_structure<VkPipelineDepthStencilStateCreateInfo>();
    bstc = make_vulkan_structure<VkPipelineColorBlendStateCreateInfo>();
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_shader_stage(VkPipelineShaderStageCreateInfo &&ss)
{
    shader_stages.push_back(ss);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_subpass(VkSubpassDescription &&sp)
{
    subpasses.push_back(sp);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_attachment(VkAttachmentDescription &&at)
{
    attachments.push_back(at);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_subpass_dependency(VkSubpassDependency &&spd)
{
    spdeps.push_back(spd);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_dynamic_state(VkDynamicState dyst)
{
    dynamic_states.push_back(dyst);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_input_binding(VkVertexInputBindingDescription &&vib)
{
    input_bindings.push_back(vib);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_vertex_attribute(VkVertexInputAttributeDescription &&via)
{
    input_attribs.push_back(via);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::primitive_topology(VkPrimitiveTopology t)
{
    iasc.topology = t;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::primitive_restart_enable(bool e)
{
    iasc.primitiveRestartEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_viewport(VkViewport &&vp)
{
    vps.push_back(vp);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_scissor_area(VkRect2D &&sa)
{
    scissors.push_back(sa);
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_clamp_enable(bool e)
{
    rstc.depthClampEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::rasterizer_discard_enable(bool e)
{
    rstc.rasterizerDiscardEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::polygon_mode(VkPolygonMode m)
{
    rstc.polygonMode = m;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::line_width(float w)
{
    rstc.lineWidth = w;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::cull_mode(VkCullModeFlags cm)
{
    rstc.cullMode = cm;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::front_face(VkFrontFace ff)
{
    rstc.frontFace = ff;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_bias_enable(bool e)
{
    rstc.depthBiasEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_test_enable(bool e)
{
    dssc.depthTestEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_write_enable(bool e)
{
    dssc.depthWriteEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_op(VkCompareOp op)
{
    dssc.depthCompareOp = op;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_bounds_test_enable(bool e)
{
    dssc.depthBoundsTestEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::depth_bounds(float min, float max)
{
    dssc.minDepthBounds = min;
    dssc.maxDepthBounds = max;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::stencil_test_enable(bool e)
{
    dssc.stencilTestEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::stencil_front_op(VkStencilOpState sop)
{
    dssc.front = sop;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::stencil_back_op(VkStencilOpState sop)
{
    dssc.back = sop;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_blending_attachment(VkPipelineColorBlendAttachmentState &&ba)
{
    blend_attachments.push_back(ba);
    bstc.attachmentCount = blend_attachments.size();
    bstc.pAttachments = blend_attachments.data();
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::blending_logic_op_enable(bool e)
{
    bstc.logicOpEnable = e;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::blending_logic_op(VkLogicOp lop)
{
    bstc.logicOp = lop;
    return *this;
}

CKVkPipelineBuilder &CKVkPipelineBuilder::add_push_constant_range(VkPushConstantRange &&pcr)
{
    push_constant_ranges.push_back(pcr);
    return *this;
}

std::tuple<VkRenderPass, VkPipelineLayout, VkPipeline> CKVkPipelineBuilder::build(VkDevice vkdev) const
{
    auto rpc = make_vulkan_structure<VkRenderPassCreateInfo>();
    rpc.attachmentCount = attachments.size();
    rpc.pAttachments = attachments.data();
    rpc.subpassCount = subpasses.size();
    rpc.pSubpasses = subpasses.data();
    rpc.dependencyCount = spdeps.size();
    rpc.pDependencies = spdeps.data();
    VkRenderPass rp;
    if (VK_SUCCESS != vkCreateRenderPass(vkdev, &rpc, nullptr, &rp))
        return std::tuple<VkRenderPass, VkPipelineLayout, VkPipeline>();

    auto dystc = make_vulkan_structure<VkPipelineDynamicStateCreateInfo>();
    dystc.dynamicStateCount = dynamic_states.size();
    dystc.pDynamicStates = dynamic_states.data();

    auto vtxinstc = make_vulkan_structure<VkPipelineVertexInputStateCreateInfo>();
    vtxinstc.vertexBindingDescriptionCount = input_bindings.size();
    vtxinstc.pVertexBindingDescriptions = input_bindings.data();
    vtxinstc.vertexAttributeDescriptionCount = input_attribs.size();
    vtxinstc.pVertexAttributeDescriptions = input_attribs.data();

    auto vpstc = make_vulkan_structure<VkPipelineViewportStateCreateInfo>();
    vpstc.viewportCount = vps.size();
    vpstc.pViewports = vps.data();
    vpstc.scissorCount = scissors.size();
    vpstc.pScissors = scissors.data();

    auto plloc = make_vulkan_structure<VkPipelineLayoutCreateInfo>();
    plloc.pushConstantRangeCount = push_constant_ranges.size();
    plloc.pPushConstantRanges = push_constant_ranges.data();
    VkPipelineLayout plo;
    if (VK_SUCCESS != vkCreatePipelineLayout(vkdev, &plloc, nullptr, &plo))
    {
        vkDestroyRenderPass(vkdev, rp, nullptr);
        return std::tuple<VkRenderPass, VkPipelineLayout, VkPipeline>();
    }

    auto plc = make_vulkan_structure<VkGraphicsPipelineCreateInfo>();
    plc.stageCount = shader_stages.size();
    plc.pStages = shader_stages.data();
    plc.pVertexInputState = &vtxinstc;
    plc.pInputAssemblyState = &iasc;
    plc.pViewportState = &vpstc;
    plc.pRasterizationState = &rstc;
    plc.pMultisampleState = &msstc;
    plc.pDepthStencilState = &dssc;
    plc.pColorBlendState = &bstc;
    plc.pDynamicState = &dystc;
    plc.layout = plo;
    plc.renderPass = rp;
    plc.subpass = 0;
    plc.basePipelineHandle = VK_NULL_HANDLE;
    plc.basePipelineIndex = -1;
    VkPipeline p;
    if (VK_SUCCESS != vkCreateGraphicsPipelines(vkdev, VK_NULL_HANDLE, 1, &plc, nullptr, &p))
    {
        vkDestroyPipelineLayout(vkdev, plo, nullptr);
        vkDestroyRenderPass(vkdev, rp, nullptr);
        return std::tuple<VkRenderPass, VkPipelineLayout, VkPipeline>();
    }

    return std::make_tuple(rp, plo, p);
}
