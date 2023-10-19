#include "ManagedVulkanPipeline.h"

ManagedVulkanPipeline::ManagedVulkanPipeline(VkRenderPass _rp, VkPipelineLayout _plo, VkPipeline _pl,
                                             std::vector<VkDescriptorSetLayout> &&_dsls, VkDevice _vkdev) :
    rp(_rp), plo(_plo), pl(_pl), dsls(_dsls), vkdev(_vkdev) {}

ManagedVulkanPipeline::~ManagedVulkanPipeline()
{
    vkDestroyPipeline(vkdev, pl, nullptr);
    vkDestroyPipelineLayout(vkdev, plo, nullptr);
    vkDestroyRenderPass(vkdev, rp, nullptr);
    for(auto &dsl : dsls)
        vkDestroyDescriptorSetLayout(vkdev, dsl, nullptr);
}

VkRenderPass ManagedVulkanPipeline::render_pass() { return rp; }

void ManagedVulkanPipeline::command_push_constants(VkCommandBuffer cmdbuf, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void *d)
{
    vkCmdPushConstants(cmdbuf, plo, stages, offset, size, d);
}

void ManagedVulkanPipeline::command_bind_descriptor_sets(VkCommandBuffer cmdbuf, VkPipelineBindPoint plbp, uint32_t first_set, uint32_t ds_count, const VkDescriptorSet *sets, uint32_t dynoff_count, const uint32_t *dynoff)
{
    vkCmdBindDescriptorSets(cmdbuf, plbp, plo, first_set, ds_count, sets, dynoff_count, dynoff);
}

void ManagedVulkanPipeline::command_bind_pipeline(VkCommandBuffer cmdbuf, VkPipelineBindPoint bp)
{
    vkCmdBindPipeline(cmdbuf, bp, pl);
}

std::vector<VkDescriptorSetLayout>& ManagedVulkanPipeline::descriptor_set_layouts()
{
    return dsls;
}
