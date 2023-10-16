#ifndef CKVKPIPELINE_H
#define CKVKPIPELINE_H

#include <tuple>
#include <vector>

#include "VulkanUtils.h"

class CKVkPipelineBuilder
{
public:
    CKVkPipelineBuilder();
    //Shaders
    CKVkPipelineBuilder &add_shader_stage(VkPipelineShaderStageCreateInfo &&ss);
    //Subpasses
    CKVkPipelineBuilder &add_subpass(VkSubpassDescription &&sp);
    CKVkPipelineBuilder &add_attachment(VkAttachmentDescription &&at);
    CKVkPipelineBuilder &add_subpass_dependency(VkSubpassDependency &&spd);
    //Dynamic States
    CKVkPipelineBuilder &add_dynamic_state(VkDynamicState dyst);
    //Input Assemblage
    CKVkPipelineBuilder &add_input_binding(VkVertexInputBindingDescription &&vib);
    CKVkPipelineBuilder &add_vertex_attribute(VkVertexInputAttributeDescription &&via);
    CKVkPipelineBuilder &primitive_topology(VkPrimitiveTopology t);
    CKVkPipelineBuilder &primitive_restart_enable(bool e);
    //Viewport & Scissor
    CKVkPipelineBuilder &add_viewport(VkViewport &&vp);
    CKVkPipelineBuilder &add_scissor_area(VkRect2D &&sa);
    //Rasterization State
    CKVkPipelineBuilder &depth_clamp_enable(bool e);
    CKVkPipelineBuilder &rasterizer_discard_enable(bool e);
    CKVkPipelineBuilder &polygon_mode(VkPolygonMode m);
    CKVkPipelineBuilder &line_width(float w);
    CKVkPipelineBuilder &cull_mode(VkCullModeFlags cm);
    CKVkPipelineBuilder &front_face(VkFrontFace ff);
    CKVkPipelineBuilder &depth_bias_enable(bool e);
    //Depth & Stencil test
    CKVkPipelineBuilder &depth_test_enable(bool e);
    CKVkPipelineBuilder &depth_write_enable(bool e);
    CKVkPipelineBuilder &depth_op(VkCompareOp op);
    CKVkPipelineBuilder &depth_bounds_test_enable(bool e);
    CKVkPipelineBuilder &depth_bounds(float min, float max);
    CKVkPipelineBuilder &stencil_test_enable(bool e);
    CKVkPipelineBuilder &stencil_front_op(VkStencilOpState sop);
    CKVkPipelineBuilder &stencil_back_op(VkStencilOpState sop);
    //Blending
    CKVkPipelineBuilder &add_blending_attachment(VkPipelineColorBlendAttachmentState &&ba);
    CKVkPipelineBuilder &blending_logic_op_enable(bool e);
    CKVkPipelineBuilder &blending_logic_op(VkLogicOp lop);
    //Pipeline Layout
    CKVkPipelineBuilder &add_push_constant_range(VkPushConstantRange &&pcr);
    //...................descriptor set layout

    std::tuple<VkRenderPass, VkPipelineLayout, VkPipeline> build(VkDevice vkdev) const;
private:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkSubpassDependency> spdeps;
    VkRenderPass vkrp;
    std::vector<VkDynamicState> dynamic_states;
    std::vector<VkVertexInputBindingDescription> input_bindings;
    std::vector<VkVertexInputAttributeDescription> input_attribs;
    VkPipelineInputAssemblyStateCreateInfo iasc;
    std::vector<VkViewport> vps;
    std::vector<VkRect2D> scissors;
    VkPipelineRasterizationStateCreateInfo rstc;
    VkPipelineMultisampleStateCreateInfo msstc;
    VkPipelineDepthStencilStateCreateInfo dssc;
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    VkPipelineColorBlendStateCreateInfo bstc;
    std::vector<VkPushConstantRange> push_constant_ranges;
};

#endif