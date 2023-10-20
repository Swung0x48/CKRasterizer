#include "CKVkTexture.h"

#include <cstring>

#include "CKVkBuffer.h"
#include "CKVkRasterizer.h"

CKVkTexture::CKVkTexture(CKVkRasterizerContext *ctx, CKTextureDesc *texdesc) :
    CKTextureDesc(*texdesc), rctx(ctx), img(0), dmem(0), imgv(0)
{
}

CKVkTexture::~CKVkTexture() {}

void CKVkTexture::create()
{
    auto ici = make_vulkan_structure<VkImageCreateInfo>({
        .imageType=VK_IMAGE_TYPE_2D,
        .format=VK_FORMAT_R8G8B8A8_SRGB,
        .extent={(uint32_t)Format.Width, (uint32_t)Format.Height, 1},
        .mipLevels=1,
        .arrayLayers=1,
        .samples=VK_SAMPLE_COUNT_1_BIT,
        .tiling=VK_IMAGE_TILING_OPTIMAL,
        .usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED
    });
    vkCreateImage(rctx->vkdev, &ici, nullptr, &img);
    VkMemoryRequirements mreq;
    vkGetImageMemoryRequirements(rctx->vkdev, img, &mreq);

    auto mai = make_vulkan_structure<VkMemoryAllocateInfo>({
        .allocationSize=mreq.size,
        .memoryTypeIndex=get_memory_type_index(mreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rctx->vkphydev)
    });
    vkAllocateMemory(rctx->vkdev, &mai, nullptr, &dmem);
    vkBindImageMemory(rctx->vkdev, img, dmem, 0);

    imgv = create_image_view(rctx->vkdev, img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void CKVkTexture::bind() {}

void CKVkTexture::load(void *data)
{
    CKVkBuffer *stgbuf = new CKVkBuffer(rctx);
    uint64_t isz = Format.BitsPerPixel / 8 * Format.Width * Format.Height;
    stgbuf->create(isz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(stgbuf->map(0, isz), data, isz);
    stgbuf->unmap();

    auto change_image_layout = [this](auto cmdbuf, VkImageLayout from, VkImageLayout to) {
        auto imb = make_vulkan_structure<VkImageMemoryBarrier>({
            .oldLayout=from,
            .newLayout=to,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .image=img,
            .subresourceRange={
                .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel=0,
                .levelCount=1,
                .baseArrayLayer=0,
                .layerCount=1
            }
        });
        VkPipelineStageFlags srcst, dstst;
        if (from == VK_IMAGE_LAYOUT_UNDEFINED && to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            imb.srcAccessMask = 0;
            imb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcst = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstst = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (from == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && to == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcst = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        vkCmdPipelineBarrier(cmdbuf, srcst, dstst, 0, 0, nullptr, 0, nullptr, 1, &imb);
    };

    run_oneshot_command_list(rctx->vkdev, rctx->cmdpool, rctx->gfxq, [=](auto cmdbuf) { change_image_layout(cmdbuf, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); });
    run_oneshot_command_list(rctx->vkdev, rctx->cmdpool, rctx->gfxq, [this, stgbuf](auto cmdbuf) {
        VkBufferImageCopy cpyr {
            .bufferOffset=0,
            .bufferRowLength=0,
            .bufferImageHeight=0,
            .imageSubresource={
                .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel=0,
                .baseArrayLayer=0,
                .layerCount=1
            },
            .imageOffset={0, 0, 0},
            .imageExtent={static_cast<uint32_t>(Format.Width), static_cast<uint32_t>(Format.Height), 1}
        };
        vkCmdCopyBufferToImage(cmdbuf, stgbuf->get_buffer(), img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpyr);
    });
    run_oneshot_command_list(rctx->vkdev, rctx->cmdpool, rctx->gfxq, [=](auto cmdbuf) { change_image_layout(cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); });

    delete stgbuf;
}
