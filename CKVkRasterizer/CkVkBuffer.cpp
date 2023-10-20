#include "CKVkBuffer.h"

#include "CKVkRasterizer.h"

CKVkBuffer::CKVkBuffer(CKVkRasterizerContext *ctx) : rctx(ctx), vkbuf(0), vkbufmem(0), vkcmdbuf(0), size(0) {}

CKVkBuffer::~CKVkBuffer()
{
    vkDestroyBuffer(rctx->vkdev, vkbuf, nullptr);
    vkFreeMemory(rctx->vkdev, vkbufmem, nullptr);
}

void CKVkBuffer::create(uint64_t sz, VkBufferUsageFlags usage, VkMemoryPropertyFlags mprop)
{
    size = sz;
    auto vkbufc = make_vulkan_structure<VkBufferCreateInfo>({
        .size=sz,
        .usage=usage,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE
    });
    vkCreateBuffer(rctx->vkdev, &vkbufc, nullptr, &vkbuf);

    VkMemoryRequirements mreq;
    vkGetBufferMemoryRequirements(rctx->vkdev, vkbuf, &mreq);
    auto alloci = make_vulkan_structure<VkMemoryAllocateInfo>({
        .allocationSize=mreq.size,
        .memoryTypeIndex=get_memory_type_index(mreq.memoryTypeBits, mprop, rctx->vkphydev)
    });
    vkAllocateMemory(rctx->vkdev, &alloci, nullptr, &vkbufmem);
    vkBindBufferMemory(rctx->vkdev, vkbuf, vkbufmem, 0);

    if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    {
        auto vkcmdbufac = make_vulkan_structure<VkCommandBufferAllocateInfo>({
            .commandPool=rctx->cmdpool,
            .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount=1
        });
        vkAllocateCommandBuffers(rctx->vkdev, &vkcmdbufac, &vkcmdbuf);
    }
}

VkBuffer CKVkBuffer::get_buffer() { return vkbuf; }

void CKVkBuffer::transfer(VkBuffer dst)
{
    auto vkcbbegin = make_vulkan_structure<VkCommandBufferBeginInfo>({.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

    vkResetCommandBuffer(vkcmdbuf, 0);
    vkBeginCommandBuffer(vkcmdbuf, &vkcbbegin);
    VkBufferCopy bcpy{};
    bcpy.srcOffset = bcpy.dstOffset = 0;
    bcpy.size = size;
    vkCmdCopyBuffer(vkcmdbuf, vkbuf, dst, 1, &bcpy);
    vkEndCommandBuffer(vkcmdbuf);

    auto submiti = make_vulkan_structure<VkSubmitInfo>({
        .commandBufferCount=1,
        .pCommandBuffers=&vkcmdbuf
    });
    vkQueueSubmit(rctx->gfxq, 1, &submiti, VK_NULL_HANDLE);
    vkQueueWaitIdle(rctx->gfxq);
}

void *CKVkBuffer::map(uint64_t offset, uint64_t size)
{
    void *ret = nullptr;
    vkMapMemory(rctx->vkdev, vkbufmem, offset, size, 0, &ret);
    return ret;
}

void CKVkBuffer::unmap() { vkUnmapMemory(rctx->vkdev, vkbufmem); }
