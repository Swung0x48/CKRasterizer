#include "CKVkIndexBuffer.h"

#include "CKVkRasterizer.h"
#include "CKVkBuffer.h"

CKVkIndexBuffer::CKVkIndexBuffer(CKIndexBufferDesc *desired_format, CKVkRasterizerContext *ctx) :
    CKIndexBufferDesc(*desired_format),
    csbuf(nullptr), ssbuf(nullptr), rctx(ctx), mapped_ptr(nullptr)
{
    m_CurrentICount = 0;
    m_Flags |= CKRST_VB_VALID;
    size = 2 * m_MaxIndexCount;
}

CKVkIndexBuffer::~CKVkIndexBuffer()
{
    if (mapped_ptr) csbuf->unmap();
    if (csbuf) delete csbuf;
    if (ssbuf) delete ssbuf;
}

void CKVkIndexBuffer::create()
{
    /*if (m_Flags & CKRST_VB_DYNAMIC)
    {
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {*/
        ssbuf = new CKVkBuffer(rctx);
        ssbuf->create(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        csbuf = new CKVkBuffer(rctx);
        csbuf->create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    //}
}

void CKVkIndexBuffer::bind(VkCommandBuffer cmdbuf)
{
    vkCmdBindIndexBuffer(cmdbuf, ssbuf->get_buffer(), 0, VK_INDEX_TYPE_UINT16);
}

void *CKVkIndexBuffer::lock(uint64_t offset, uint64_t size)
{
    if (mapped_ptr) return mapped_ptr;
    if (m_Flags & CKRST_VB_DYNAMIC)
        return mapped_ptr = csbuf->map(offset, size ? size : this->size);
    else
        return csbuf->map(offset, size ? size : this->size);
}

void CKVkIndexBuffer::unlock()
{
    if (m_Flags & CKRST_VB_DYNAMIC)
    {
        csbuf->transfer_with_barrier(ssbuf->get_buffer(), VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT);
    }
    else
    {
        csbuf->unmap();
        csbuf->transfer(ssbuf->get_buffer());
    }
    //else ssbuf->unmap();
}
