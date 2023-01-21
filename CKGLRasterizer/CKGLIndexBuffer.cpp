#include "CKGLIndexBuffer.h"
#include "CKGLRasterizer.h"

CKGLIndexBuffer::CKGLIndexBuffer(CKIndexBufferDesc *DesiredFormat)
{
    GLBuffer = 0;
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount; // Max number of indices this buffer can contain
    this->m_CurrentICount = DesiredFormat->m_CurrentICount; // For dynamic buffers, current number of indices taken in this buffer
    lock_offset = 0;
    lock_length = 0;
}

CKGLIndexBuffer::~CKGLIndexBuffer()
{
    glDeleteBuffers(1, &GLBuffer);
    GLBuffer = 0;
    if (client_side_data)
        VirtualFree(client_side_data, 0, MEM_RELEASE);
    client_side_data = nullptr;
    if (client_side_locked_data)
        VirtualFree(client_side_locked_data, 0, MEM_RELEASE);
    client_side_locked_data = nullptr;
}


bool CKGLIndexBuffer::operator==(const CKIndexBufferDesc & that) const
{
    return
        this->m_Flags == that.m_Flags &&
        this->m_CurrentICount == that.m_CurrentICount &&
        this->m_MaxIndexCount == that.m_MaxIndexCount;
}

void CKGLIndexBuffer::Create()
{
    glCreateBuffers(1, &GLBuffer);
    GLenum flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
    if (!(m_Flags & CKRST_VB_WRITEONLY)) //virtools header says this bit is always set, but just in case...
        flags |= GL_MAP_READ_BIT;
    if (!(m_Flags & CKRST_VB_DYNAMIC))
    {
        client_side_data = VirtualAlloc(nullptr, 2 * m_MaxIndexCount, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    }
    else
        glNamedBufferStorage(GLBuffer, 2 * this->m_MaxIndexCount, NULL, flags);
    m_Flags |= CKRST_VB_VALID;
}

void CKGLIndexBuffer::Bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLBuffer);
}

void *CKGLIndexBuffer::Lock(CKDWORD offset, CKDWORD len, bool overwrite)
{
    ZoneScopedN(__FUNCTION__);
    if (!offset && !len)
    {
        len = m_MaxIndexCount * 2;
    }
    if (client_side_data)
    {
        lock_offset = offset;
        lock_length = len;
        if (offset == 0 && len == m_MaxIndexCount * 2)
            return client_side_data;
        client_side_locked_data = VirtualAlloc(nullptr, len, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
        return client_side_locked_data;
    }
    void* ret = nullptr;
    ret = glMapNamedBufferRange(GLBuffer, offset, len, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | (overwrite ? 0 : GL_MAP_UNSYNCHRONIZED_BIT));
    return ret;
}

void CKGLIndexBuffer::Unlock()
{
    if (client_side_data)
    {
        size_t x[8], c = 8; DWORD _g;
        if (!client_side_locked_data) //the entire buffer locked
        {
            GetWriteWatch(WRITE_WATCH_FLAG_RESET, client_side_data, lock_length, (void**)&x, (ULONG_PTR*)&c, &_g);
            if (c > 0) {
                glNamedBufferData(GLBuffer, 2 * m_MaxIndexCount, client_side_data, GL_STATIC_DRAW);
            }
        }
        else
        {
            GetWriteWatch(WRITE_WATCH_FLAG_RESET, client_side_locked_data, lock_length, (void**)&x, (ULONG_PTR*)&c, &_g);
            if (c > 0)
            {
                memcpy((uint8_t*)client_side_data + lock_offset, client_side_locked_data, lock_length);
                glNamedBufferData(GLBuffer, 2 * m_MaxIndexCount, client_side_data, GL_STATIC_DRAW);
            }
            VirtualFree(client_side_locked_data, 0, MEM_RELEASE);
            client_side_locked_data = nullptr;
        }
        lock_offset = ~0U;
        lock_length = 0;
        return;
    }
    int locked = 0;
    glGetNamedBufferParameteriv(GLBuffer, GL_BUFFER_MAPPED, &locked);
    if (!locked) return;
    glUnmapNamedBuffer(GLBuffer);
}