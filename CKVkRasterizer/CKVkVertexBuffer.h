#ifndef CKVKVERTEXBUFFER_H
#define CKVKVERTEXBUFFER_H

#include <CKRasterizerTypes.h>

#include "VulkanUtils.h"

class CKVkRasterizerContext;
class CKVkBuffer;

struct CKVkVertexBuffer : public CKVertexBufferDesc
{
private:
    CKVkBuffer *csbuf;
    CKVkBuffer *ssbuf;
    CKVkRasterizerContext *rctx;
    uint64_t size;
public:
    explicit CKVkVertexBuffer(CKVertexBufferDesc *desired_format, CKVkRasterizerContext *ctx);
    ~CKVkVertexBuffer();

    void create();
    void bind(VkCommandBuffer cmdbuf);
    void* lock(uint64_t offset, uint64_t size);
    void unlock();
};

#endif