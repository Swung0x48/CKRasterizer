#include "VkPipelineBuilder.h"

VkPipelineBuilder::VkPipelineBuilder() :
    vkrp{}
{
    iasc = make_vulkan_structure<VkPipelineInputAssemblyStateCreateInfo>({});
    rstc = make_vulkan_structure<VkPipelineRasterizationStateCreateInfo>({});
    msstc = make_vulkan_structure<VkPipelineMultisampleStateCreateInfo>({
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.,
        .pSampleMask=nullptr,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    });
    dssc = make_vulkan_structure<VkPipelineDepthStencilStateCreateInfo>({});
    bstc = make_vulkan_structure<VkPipelineColorBlendStateCreateInfo>({});
}

VkPipelineBuilder &VkPipelineBuilder::add_shader_stage(VkPipelineShaderStageCreateInfo &&ss)
{
    shader_stages.push_back(ss);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_subpass(VkSubpassDescription &&sp)
{
    subpasses.push_back(sp);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_attachment(VkAttachmentDescription &&at)
{
    attachments.push_back(at);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_subpass_dependency(VkSubpassDependency &&spd)
{
    spdeps.push_back(spd);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_dynamic_state(VkDynamicState dyst)
{
    dynamic_states.push_back(dyst);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_input_binding(VkVertexInputBindingDescription &&vib)
{
    input_bindings.push_back(vib);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_vertex_attribute(VkVertexInputAttributeDescription &&via)
{
    input_attribs.push_back(via);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::primitive_topology(VkPrimitiveTopology t)
{
    iasc.topology = t;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::primitive_restart_enable(bool e)
{
    iasc.primitiveRestartEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_viewport(VkViewport &&vp)
{
    vps.push_back(vp);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_scissor_area(VkRect2D &&sa)
{
    scissors.push_back(sa);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::set_fixed_viewport_count(uint32_t c)
{
    vpc.emplace(c);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::set_fixed_scissor_count(uint32_t c)
{
    scc.emplace(c);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_clamp_enable(bool e)
{
    rstc.depthClampEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::rasterizer_discard_enable(bool e)
{
    rstc.rasterizerDiscardEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::polygon_mode(VkPolygonMode m)
{
    rstc.polygonMode = m;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::line_width(float w)
{
    rstc.lineWidth = w;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::cull_mode(VkCullModeFlags cm)
{
    rstc.cullMode = cm;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::front_face(VkFrontFace ff)
{
    rstc.frontFace = ff;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_bias_enable(bool e)
{
    rstc.depthBiasEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_test_enable(bool e)
{
    dssc.depthTestEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_write_enable(bool e)
{
    dssc.depthWriteEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_op(VkCompareOp op)
{
    dssc.depthCompareOp = op;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_bounds_test_enable(bool e)
{
    dssc.depthBoundsTestEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::depth_bounds(float min, float max)
{
    dssc.minDepthBounds = min;
    dssc.maxDepthBounds = max;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::stencil_test_enable(bool e)
{
    dssc.stencilTestEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::stencil_front_op(VkStencilOpState sop)
{
    dssc.front = sop;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::stencil_back_op(VkStencilOpState sop)
{
    dssc.back = sop;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_blending_attachment(VkPipelineColorBlendAttachmentState &&ba)
{
    blend_attachments.push_back(ba);
    bstc.attachmentCount = blend_attachments.size();
    bstc.pAttachments = blend_attachments.data();
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::blending_logic_op_enable(bool e)
{
    bstc.logicOpEnable = e;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::blending_logic_op(VkLogicOp lop)
{
    bstc.logicOp = lop;
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_push_constant_range(VkPushConstantRange &&pcr)
{
    push_constant_ranges.push_back(pcr);
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::new_descriptor_set_layout(VkDescriptorSetLayoutCreateFlags flags)
{
    desc_sets.emplace_back(flags, std::vector<VkDescriptorSetLayoutBinding>{});
    return *this;
}

VkPipelineBuilder &VkPipelineBuilder::add_descriptor_set_binding(VkDescriptorSetLayoutBinding &&b)
{
    if (!desc_sets.empty())
        desc_sets.back().second.push_back(b);
    return *this;
}

ManagedVulkanPipeline* VkPipelineBuilder::build(VkDevice vkdev) const
{
    auto rpc = make_vulkan_structure<VkRenderPassCreateInfo>({
        .attachmentCount=attachments.size(),
        .pAttachments=attachments.data(),
        .subpassCount=subpasses.size(),
        .pSubpasses=subpasses.data(),
        .dependencyCount=spdeps.size(),
        .pDependencies=spdeps.data()
    });
    VkRenderPass rp;
    if (VK_SUCCESS != vkCreateRenderPass(vkdev, &rpc, nullptr, &rp))
        return nullptr;

    auto dystc = make_vulkan_structure<VkPipelineDynamicStateCreateInfo>({
        .dynamicStateCount=dynamic_states.size(),
        .pDynamicStates=dynamic_states.data()
    });

    auto vtxinstc = make_vulkan_structure<VkPipelineVertexInputStateCreateInfo>({
        .vertexBindingDescriptionCount=input_bindings.size(),
        .pVertexBindingDescriptions=input_bindings.data(),
        .vertexAttributeDescriptionCount=input_attribs.size(),
        .pVertexAttributeDescriptions=input_attribs.data()
    });

    auto vpstc = make_vulkan_structure<VkPipelineViewportStateCreateInfo>({
        .viewportCount=vpc.value_or(vps.size()),
        .pViewports=vps.data(),
        .scissorCount=scc.value_or(scissors.size()),
        .pScissors=scissors.data()
    });

    std::vector<VkDescriptorSetLayout> dsls;
    for (auto& ds_desc : desc_sets)
    {
        auto dslc = make_vulkan_structure<VkDescriptorSetLayoutCreateInfo>({
            .flags=ds_desc.first,
            .bindingCount=ds_desc.second.size(),
            .pBindings=ds_desc.second.data()
        });
        VkDescriptorSetLayout layout;
        if (VK_SUCCESS != vkCreateDescriptorSetLayout(vkdev, &dslc, nullptr, &layout))
        {
            vkDestroyRenderPass(vkdev, rp, nullptr);
            for (auto &dsl : dsls)
                vkDestroyDescriptorSetLayout(vkdev, dsl, nullptr);
            return nullptr;
        }
        dsls.push_back(layout);
    }

    auto plloc = make_vulkan_structure<VkPipelineLayoutCreateInfo>({
        .setLayoutCount=dsls.size(),
        .pSetLayouts=dsls.data(),
        .pushConstantRangeCount=push_constant_ranges.size(),
        .pPushConstantRanges=push_constant_ranges.data()
    });

    VkPipelineLayout plo;
    if (VK_SUCCESS != vkCreatePipelineLayout(vkdev, &plloc, nullptr, &plo))
    {
        vkDestroyRenderPass(vkdev, rp, nullptr);
        for (auto &dsl : dsls)
            vkDestroyDescriptorSetLayout(vkdev, dsl, nullptr);
        return nullptr;
    }

    auto plc = make_vulkan_structure<VkGraphicsPipelineCreateInfo>({
        .stageCount=shader_stages.size(),
        .pStages=shader_stages.data(),
        .pVertexInputState=&vtxinstc,
        .pInputAssemblyState=&iasc,
        .pViewportState=&vpstc,
        .pRasterizationState=&rstc,
        .pMultisampleState=&msstc,
        .pDepthStencilState=&dssc,
        .pColorBlendState=&bstc,
        .pDynamicState=&dystc,
        .layout=plo,
        .renderPass=rp,
        .subpass=0,
        .basePipelineHandle=VK_NULL_HANDLE,
        .basePipelineIndex=-1
    });
    VkPipeline p;
    if (VK_SUCCESS != vkCreateGraphicsPipelines(vkdev, VK_NULL_HANDLE, 1, &plc, nullptr, &p))
    {
        vkDestroyPipelineLayout(vkdev, plo, nullptr);
        vkDestroyRenderPass(vkdev, rp, nullptr);
        for (auto &dsl : dsls)
            vkDestroyDescriptorSetLayout(vkdev, dsl, nullptr);
        return nullptr;
    }

    return new ManagedVulkanPipeline(rp, plo, p, std::move(dsls), vkdev);
}
