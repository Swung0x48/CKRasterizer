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

void run_oneshot_command_list(VkDevice dev, VkCommandPool cmdp, VkQueue q, std::function<void(VkCommandBuffer)> f)
{
    VkCommandBuffer cmdbuf;
    auto vkcmdbufac = make_vulkan_structure<VkCommandBufferAllocateInfo>({
        .commandPool=cmdp,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1
    });
    vkAllocateCommandBuffers(dev, &vkcmdbufac, &cmdbuf);

    auto cbbi = make_vulkan_structure<VkCommandBufferBeginInfo>({.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});
    vkResetCommandBuffer(cmdbuf, 0);

    vkBeginCommandBuffer(cmdbuf, &cbbi);
    f(cmdbuf);
    vkEndCommandBuffer(cmdbuf);

    auto submiti = make_vulkan_structure<VkSubmitInfo>({
        .commandBufferCount=1,
        .pCommandBuffers=&cmdbuf
    });
    vkQueueSubmit(q, 1, &submiti, VK_NULL_HANDLE);
    vkQueueWaitIdle(q);
    vkFreeCommandBuffers(dev, cmdp, 1, &cmdbuf);
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

CKVkMemoryImage create_memory_image(VkDevice vkdev, VkPhysicalDevice vkphydev, uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memprop)
{
    CKVkMemoryImage ret{NULL, NULL};
    auto imci = make_vulkan_structure<VkImageCreateInfo>({
        .imageType = VK_IMAGE_TYPE_2D,
        .format = fmt,
        .extent = {.width=w, .height=h, .depth=1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    });
    if (vkCreateImage(vkdev, &imci, nullptr, &ret.im) != VK_SUCCESS)
        return ret;

    VkMemoryRequirements mreq;
    vkGetImageMemoryRequirements(vkdev, ret.im, &mreq);
    auto alloci = make_vulkan_structure<VkMemoryAllocateInfo>({
        .allocationSize=mreq.size,
        .memoryTypeIndex=get_memory_type_index(mreq.memoryTypeBits, memprop, vkphydev)
    });
    if (vkAllocateMemory(vkdev, &alloci, nullptr, &ret.mem) != VK_SUCCESS)
        return ret;
    vkBindImageMemory(vkdev, ret.im, ret.mem, 0);
    return ret;
}

void destroy_memory_image(VkDevice vkdev, CKVkMemoryImage i)
{
    vkDestroyImage(vkdev, i.im, nullptr);
    vkFreeMemory(vkdev, i.mem, nullptr);
}

VkImageView create_image_view(VkDevice vkdev, VkImage img, VkFormat fmt, VkImageAspectFlags aspf)
{
    auto ivci = make_vulkan_structure<VkImageViewCreateInfo>({
        .image=img,
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=fmt,
        .subresourceRange={
            .aspectMask=aspf,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
    }});

    VkImageView ret;
    vkCreateImageView(vkdev, &ivci, nullptr, &ret); // != VK_SUCCESS...
    return ret;
}