#include "CKVkIndexBuffer.h"

#include "CKVkRasterizer.h"
#include "CKVkBuffer.h"

CKVkIndexBuffer::CKVkIndexBuffer(CKIndexBufferDesc *desired_format, CKVkRasterizerContext *ctx) :
    CKIndexBufferDesc(*desired_format),
    csbuf(nullptr), ssbuf(nullptr), rctx(ctx)
{
    m_CurrentICount = 0;
    m_Flags |= CKRST_VB_VALID;
    size = 2 * m_MaxIndexCount;
}

CKVkIndexBuffer::~CKVkIndexBuffer()
{
    if (csbuf) delete csbuf;
    if (ssbuf) delete ssbuf;
}

void CKVkIndexBuffer::create()
{
    if (m_Flags & CKRST_VB_DYNAMIC)
    {
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        csbuf = new CKVkBuffer(rctx);
        csbuf->create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void CKVkIndexBuffer::bind(VkCommandBuffer cmdbuf)
{
    vkCmdBindIndexBuffer(cmdbuf, ssbuf->get_buffer(), 0, VK_INDEX_TYPE_UINT16);
}

void *CKVkIndexBuffer::lock(uint64_t offset, uint64_t size)
{
    if (csbuf)
        return csbuf->map(offset, size ? size : this->size);
    else
        return ssbuf->map(offset, size ? size : this->size);
}

void CKVkIndexBuffer::unlock()
{
    if (csbuf)
    {
        csbuf->unmap();
        csbuf->transfer(ssbuf->get_buffer());
    }
    else ssbuf->unmap();
}
