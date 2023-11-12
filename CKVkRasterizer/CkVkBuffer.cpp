#include "CKVkBuffer.h"

#include "CKVkRasterizer.h"

CKVkBuffer::CKVkBuffer(CKVkRasterizerContext *ctx) : rctx(ctx), vkbuf(0), vkbufmem(0), size(0) {}

CKVkBuffer::~CKVkBuffer()
{
    //buffer deallocation should be scheduled to happen after command buffer execution has been completed
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
}

VkBuffer CKVkBuffer::get_buffer() { return vkbuf; }

void CKVkBuffer::transfer(VkBuffer dst)
{
    run_oneshot_command_list(rctx->vkdev, rctx->cmdpool, rctx->gfxq, [this, &dst](auto cmdbuf) {
        VkBufferCopy bcpy{};
        bcpy.srcOffset = bcpy.dstOffset = 0;
        bcpy.size = size;
        vkCmdCopyBuffer(cmdbuf, vkbuf, dst, 1, &bcpy);
    });
}

void CKVkBuffer::transfer_with_barrier(VkBuffer dst, VkPipelineStageFlags dststage, VkAccessFlags dstaccessmask)
{
    run_oneshot_command_list(rctx->vkdev, rctx->cmdpool, rctx->gfxq, [&, this](auto cmdbuf) {
        VkBufferCopy bcpy{};
        bcpy.srcOffset = bcpy.dstOffset = 0;
        bcpy.size = size;
        vkCmdCopyBuffer(cmdbuf, vkbuf, dst, 1, &bcpy);
        auto mb = make_vulkan_structure<VkMemoryBarrier>({
            .srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask=dstaccessmask
        });
        vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, dststage, 0, 1, &mb, 0, nullptr, 0, nullptr);
    });
}

void *CKVkBuffer::map(uint64_t offset, uint64_t size)
{
    void *ret = nullptr;
    vkMapMemory(rctx->vkdev, vkbufmem, offset, size, 0, &ret);
    return ret;
}

void CKVkBuffer::unmap() { vkUnmapMemory(rctx->vkdev, vkbufmem); }
