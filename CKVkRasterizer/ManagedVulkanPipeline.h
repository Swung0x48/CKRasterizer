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

    void command_push_constants(VkCommandBuffer cmdbuf, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* d);
    void command_bind_descriptor_sets(VkCommandBuffer cmdbuf, VkPipelineBindPoint plbp, uint32_t first_set, uint32_t ds_count, const VkDescriptorSet* sets, uint32_t dynoff_count, const uint32_t* dynoff);
    void command_bind_pipeline(VkCommandBuffer cmdbuf, VkPipelineBindPoint bp);

    std::vector<VkDescriptorSetLayout>& descriptor_set_layouts();
private:
    ManagedVulkanPipeline(VkRenderPass _rp, VkPipelineLayout _plo, VkPipeline _pl, std::vector<VkDescriptorSetLayout> &&_dsls, VkDevice _vkdev);
    VkRenderPass rp;
    VkPipelineLayout plo;
    VkPipeline pl;
    std::vector<VkDescriptorSetLayout> dsls;
    VkDevice vkdev;

    friend class VkPipelineBuilder;
};

#endif