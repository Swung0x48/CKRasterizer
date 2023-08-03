#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define VULKANUTILS_IMPL

#include <CKRasterizer.h>

#include "VulkanUtils.h"

std::vector<VkVertexInputAttributeDescription> rst_vertex_format_to_vulkan_vertex_attrib(CKRST_VERTEXFORMAT vf)
{
    std::vector<VkVertexInputAttributeDescription> ret;
    uint32_t offset = 0;
    if (vf & CKRST_VF_POSITION)
    {
        ret.emplace_back(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offset); //give it garbage data in w, we don't care
        offset += 3 * sizeof(float);
    }
    else if (vf & CKRST_VF_RASTERPOS)
    {
        ret.emplace_back(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offset);
        offset += 4 * sizeof(float);
    }

    if (vf & CKRST_VF_NORMAL)
    {
        ret.emplace_back(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offset);
        offset += 3 * sizeof(float);
    }
    else
    {
        if (vf & CKRST_VF_DIFFUSE)
        {
            ret.emplace_back(2, 0, VK_FORMAT_B8G8R8A8_UNORM, offset);
            offset += sizeof(DWORD);
        }
        if (vf & CKRST_VF_SPECULAR)
        {
            ret.emplace_back(3, 0, VK_FORMAT_B8G8R8A8_UNORM, offset);
            offset += sizeof(DWORD);
        }
    }

    if (vf & CKRST_VF_TEX1)
    {
        ret.emplace_back(4, 0, VK_FORMAT_R32G32_SFLOAT, offset);
        offset += 2 * sizeof(float);
    }
    else if (vf & CKRST_VF_TEX2)
    {
        ret.emplace_back(4, 0, VK_FORMAT_R32G32_SFLOAT, offset);
        ret.emplace_back(5, 0, VK_FORMAT_R32G32_SFLOAT, offset);
        offset += 4 * sizeof(float);
    }

    return ret;
}

VkVertexInputBindingDescription rst_vertex_format_to_vulkan_input_binding(CKRST_VERTEXFORMAT vf)
{
    VkVertexInputBindingDescription ret{};
    ret.binding = 0;
    ret.stride = CKRSTGetVertexSize(vf);
    ret.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return ret;
}

uint32_t get_memory_type_index(uint32_t wanted_type, VkMemoryPropertyFlags prop, VkPhysicalDevice vkphydev)
{
    VkPhysicalDeviceMemoryProperties mprop;
    vkGetPhysicalDeviceMemoryProperties(vkphydev, &mprop);
    for (uint32_t i = 0; i < mprop.memoryTypeCount; ++i)
    {
        if ((wanted_type & (1 << i)) && ((mprop.memoryTypes[i].propertyFlags & prop) == prop))
            return i;
    }
    return ~0U;
}
