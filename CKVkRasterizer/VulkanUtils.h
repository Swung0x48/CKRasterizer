#ifndef VULKANUTILS_H
#define VULKANUTILS_H

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <functional>
#include <vector>

#include <CKRasterizerEnums.h>

template <typename T>
T make_vulkan_structure(T _)
{
    static_assert(sizeof(T) == 0, "unimplemented");
}

#ifndef VULKANUTILS_IMPL
#define define_vk_typed_structure(ty, _) \
template <> ty make_vulkan_structure<ty>(ty _);
#else
#define define_vk_typed_structure(ty, sty) \
template <> ty make_vulkan_structure<ty>(ty r) {\
    r.sType = sty;\
    return r;\
}
#endif

define_vk_typed_structure(VkApplicationInfo, VK_STRUCTURE_TYPE_APPLICATION_INFO)
define_vk_typed_structure(VkDebugUtilsMessengerCreateInfoEXT, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT)
define_vk_typed_structure(VkInstanceCreateInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
define_vk_typed_structure(VkDeviceQueueCreateInfo, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
define_vk_typed_structure(VkDeviceCreateInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
define_vk_typed_structure(VkWin32SurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR)
define_vk_typed_structure(VkSwapchainCreateInfoKHR, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR)
define_vk_typed_structure(VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
define_vk_typed_structure(VkShaderModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO)
define_vk_typed_structure(VkPipelineShaderStageCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)
define_vk_typed_structure(VkRenderPassCreateInfo, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO)
define_vk_typed_structure(VkPipelineDynamicStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineVertexInputStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineInputAssemblyStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineViewportStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineRasterizationStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineMultisampleStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineColorBlendStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO)
define_vk_typed_structure(VkPipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO)
define_vk_typed_structure(VkGraphicsPipelineCreateInfo, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)
define_vk_typed_structure(VkFramebufferCreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO)
define_vk_typed_structure(VkCommandPoolCreateInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO)
define_vk_typed_structure(VkCommandBufferAllocateInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO)
define_vk_typed_structure(VkSemaphoreCreateInfo, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO)
define_vk_typed_structure(VkFenceCreateInfo, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO)
define_vk_typed_structure(VkCommandBufferBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO)
define_vk_typed_structure(VkRenderPassBeginInfo, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO)
define_vk_typed_structure(VkSubmitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO)
define_vk_typed_structure(VkPresentInfoKHR, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR)
define_vk_typed_structure(VkBufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO)
define_vk_typed_structure(VkMemoryAllocateInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO)
define_vk_typed_structure(VkDescriptorSetLayoutCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)
define_vk_typed_structure(VkImageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
define_vk_typed_structure(VkPipelineDepthStencilStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO)
define_vk_typed_structure(VkDescriptorPoolCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO)
define_vk_typed_structure(VkDescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO)
define_vk_typed_structure(VkWriteDescriptorSet, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
define_vk_typed_structure(VkImageMemoryBarrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)

struct CKVkMemoryImage
{
    VkImage im;
    VkDeviceMemory mem;
};

std::vector<VkVertexInputAttributeDescription> rst_vertex_format_to_vulkan_vertex_attrib(CKRST_VERTEXFORMAT vf);
VkVertexInputBindingDescription rst_vertex_format_to_vulkan_input_binding(CKRST_VERTEXFORMAT vf);

void run_oneshot_command_list(VkDevice dev, VkCommandPool cmdp, VkQueue q, std::function<void(VkCommandBuffer)> f);

uint32_t get_memory_type_index(uint32_t wanted_type, VkMemoryPropertyFlags prop, VkPhysicalDevice vkphydev);

CKVkMemoryImage create_memory_image(VkDevice vkdev, VkPhysicalDevice vkphydev, uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memprop);
void destroy_memory_image(VkDevice vkdev, CKVkMemoryImage i);

VkImageView create_image_view(VkDevice vkdev, VkImage img, VkFormat fmt, VkImageAspectFlags aspf);

#endif