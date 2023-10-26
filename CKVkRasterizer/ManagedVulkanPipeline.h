#ifndef MANAGEDVULKANPIPELINE_H
#define MANAGEDVULKANPIPELINE_H

#include <vector>

#include "VulkanUtils.h"

class ManagedVulkanPipeline
{
public:
    ~ManagedVulkanPipeline();
    ManagedVulkanPipeline(ManagedVulkanPipeline &) = delete;

    VkRenderPass render_pass();
    VkPipelineLayout pipeline_layout();

    void command_push_constants(VkCommandBuffer cmdbuf, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* d);
    void command_bind_descriptor_sets(VkCommandBuffer cmdbuf, VkPipelineBindPoint plbp, uint32_t first_set, uint32_t ds_count, const VkDescriptorSet* sets, uint32_t dynoff_count, const uint32_t* dynoff);
    void command_bind_pipeline(VkCommandBuffer cmdbuf, VkPipelineBindPoint bp);

    std::vector<VkDescriptorSetLayout>& descriptor_set_layouts();
private:
    ManagedVulkanPipeline(VkRenderPass _rp, bool _rp_shared, VkPipelineLayout _plo, bool _plo_shared, VkPipeline _pl, std::vector<VkDescriptorSetLayout> &&_dsls, VkDevice _vkdev);
    VkRenderPass rp;
    bool rp_shared;
    VkPipelineLayout plo;
    bool plo_shared;
    VkPipeline pl;
    std::vector<VkDescriptorSetLayout> dsls;
    VkDevice vkdev;

    friend class VkPipelineBuilder;
};

#endif