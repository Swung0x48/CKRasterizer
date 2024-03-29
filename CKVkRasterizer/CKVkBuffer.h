#ifndef CKVKBUFFER_H
#define CKVKBUFFER_H

#include <CKRasterizerTypes.h>

#include "VulkanUtils.h"

class CKVkRasterizerContext;

struct CKVkBuffer
{
private:
    VkBuffer vkbuf;
    VkDeviceMemory vkbufmem;
    CKVkRasterizerContext *rctx;
    uint64_t size;
public:
    explicit CKVkBuffer(CKVkRasterizerContext *ctx);
    ~CKVkBuffer();

    void create(uint64_t sz, VkBufferUsageFlags usage, VkMemoryPropertyFlags mprop);
    VkBuffer get_buffer();
    void transfer(VkBuffer dst);
    void transfer_with_barrier(VkBuffer dst, VkPipelineStageFlags dststage, VkAccessFlags dstaccessmask);
    void* map(uint64_t offset, uint64_t size);
    void unmap();
};

#endif