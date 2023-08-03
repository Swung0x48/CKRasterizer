#ifndef CKVKINDEXBUFFER_H
#define CKVKINDEXBUFFER_H

#include <CKRasterizerTypes.h>

#include "VulkanUtils.h"

class CKVkRasterizerContext;
class CKVkBuffer;

struct CKVkIndexBuffer : public CKIndexBufferDesc
{
private:
    CKVkBuffer *csbuf;
    CKVkBuffer *ssbuf;
    CKVkRasterizerContext *rctx;
    uint64_t size;
public:
    explicit CKVkIndexBuffer(CKIndexBufferDesc *desired_format, CKVkRasterizerContext *ctx);
    ~CKVkIndexBuffer();

    void create();
    void bind(VkCommandBuffer cmdbuf);
    void* lock(uint64_t offset, uint64_t size);
    void unlock();
};

#endif