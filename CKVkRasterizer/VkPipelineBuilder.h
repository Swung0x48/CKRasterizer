#ifndef VKPIPELINE_H
#define VKPIPELINE_H

#include <tuple>
#include <optional>
#include <vector>

#include "ManagedVulkanPipeline.h"
#include "VulkanUtils.h"

class VkPipelineBuilder
{
public:
    VkPipelineBuilder();
    //Shaders
    VkPipelineBuilder &add_shader_stage(VkPipelineShaderStageCreateInfo &&ss);
    //Render Pass
    VkPipelineBuilder &add_subpass(VkSubpassDescription &&sp);
    VkPipelineBuilder &add_attachment(VkAttachmentDescription &&at);
    VkPipelineBuilder &add_subpass_dependency(VkSubpassDependency &&spd);
    //OR use external shared render pass
    VkPipelineBuilder &existing_render_pass(VkRenderPass rp);
    //Dynamic States
    VkPipelineBuilder &add_dynamic_state(VkDynamicState dyst);
    //Input Assemblage
    VkPipelineBuilder &add_input_binding(VkVertexInputBindingDescription &&vib);
    VkPipelineBuilder &add_vertex_attribute(VkVertexInputAttributeDescription &&via);
    VkPipelineBuilder &primitive_topology(VkPrimitiveTopology t);
    VkPipelineBuilder &primitive_restart_enable(bool e);
    //Viewport & Scissor
    VkPipelineBuilder &add_viewport(VkViewport &&vp);
    VkPipelineBuilder &add_scissor_area(VkRect2D &&sa);
    VkPipelineBuilder &set_fixed_viewport_count(uint32_t c);
    VkPipelineBuilder &set_fixed_scissor_count(uint32_t c);
    //Rasterization State
    VkPipelineBuilder &depth_clamp_enable(bool e);
    VkPipelineBuilder &rasterizer_discard_enable(bool e);
    VkPipelineBuilder &polygon_mode(VkPolygonMode m);
    VkPipelineBuilder &line_width(float w);
    VkPipelineBuilder &cull_mode(VkCullModeFlags cm);
    VkPipelineBuilder &front_face(VkFrontFace ff);
    VkPipelineBuilder &depth_bias_enable(bool e);
    //Depth & Stencil test
    VkPipelineBuilder &depth_test_enable(bool e);
    VkPipelineBuilder &depth_write_enable(bool e);
    VkPipelineBuilder &depth_op(VkCompareOp op);
    VkPipelineBuilder &depth_bounds_test_enable(bool e);
    VkPipelineBuilder &depth_bounds(float min, float max);
    VkPipelineBuilder &stencil_test_enable(bool e);
    VkPipelineBuilder &stencil_front_op(VkStencilOpState sop);
    VkPipelineBuilder &stencil_back_op(VkStencilOpState sop);
    //Blending
    VkPipelineBuilder &add_blending_attachment(VkPipelineColorBlendAttachmentState &&ba);
    VkPipelineBuilder &blending_logic_op_enable(bool e);
    VkPipelineBuilder &blending_logic_op(VkLogicOp lop);
    //Pipeline Layout
    VkPipelineBuilder &add_push_constant_range(VkPushConstantRange &&pcr);
    VkPipelineBuilder &new_descriptor_set_layout(VkDescriptorSetLayoutCreateFlags flags, const void* dslc_pnext);
    VkPipelineBuilder &add_descriptor_set_binding(VkDescriptorSetLayoutBinding &&b);
    //OR use external shared pipeline layout
    VkPipelineBuilder &existing_pipeline_layout(VkPipelineLayout plo);

    ManagedVulkanPipeline* build(VkDevice vkdev) const;
private:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkSubpassDependency> spdeps;
    VkRenderPass erp;
    std::vector<VkDynamicState> dynamic_states;
    std::vector<VkVertexInputBindingDescription> input_bindings;
    std::vector<VkVertexInputAttributeDescription> input_attribs;
    VkPipelineInputAssemblyStateCreateInfo iasc;
    std::vector<VkViewport> vps;
    std::vector<VkRect2D> scissors;
    std::optional<uint32_t> vpc;
    std::optional<uint32_t> scc;
    VkPipelineRasterizationStateCreateInfo rstc;
    VkPipelineMultisampleStateCreateInfo msstc;
    VkPipelineDepthStencilStateCreateInfo dssc;
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    VkPipelineColorBlendStateCreateInfo bstc;
    std::vector<VkPushConstantRange> push_constant_ranges;
    std::vector<std::tuple<VkDescriptorSetLayoutCreateFlags, const void*, std::vector<VkDescriptorSetLayoutBinding>>> desc_sets;
    VkPipelineLayout eplo;
};

#endif