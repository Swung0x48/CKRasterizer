#include "CKVkVertexBuffer.h"

#include "CKVkRasterizer.h"
#include "CKVkBuffer.h"

CKVkVertexBuffer::CKVkVertexBuffer(CKVertexBufferDesc *desired_format, CKVkRasterizerContext *ctx) :
    CKVertexBufferDesc(*desired_format),
    csbuf(nullptr), ssbuf(nullptr), rctx(ctx)
{
    m_CurrentVCount = 0;
    m_Flags |= CKRST_VB_VALID;
    size = CKRSTGetVertexSize(m_VertexFormat) * m_MaxVertexCount;
}

CKVkVertexBuffer::~CKVkVertexBuffer()
{
    if (csbuf) delete csbuf;
    if (ssbuf) delete ssbuf;
}

void CKVkVertexBuffer::create()
{
    if (m_Flags & CKRST_VB_DYNAMIC)
    {
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        csbuf = new CKVkBuffer(rctx);
        csbuf->create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void CKVkVertexBuffer::bind(VkCommandBuffer cmdbuf)
{
    VkBuffer b[] = {ssbuf->get_buffer()};
    uint64_t o[] = {0};
    vkCmdBindVertexBuffers(cmdbuf, 0, 1, b, o);
}

void *CKVkVertexBuffer::lock(uint64_t offset, uint64_t size)
{
    if (csbuf)
        return csbuf->map(offset, size ? size : this->size);
    else
        return ssbuf->map(offset, size ? size : this->size);
}

void CKVkVertexBuffer::unlock()
{
    if (csbuf)
    {
        csbuf->unmap();
        csbuf->transfer(ssbuf->get_buffer());
    }
    else ssbuf->unmap();
}
