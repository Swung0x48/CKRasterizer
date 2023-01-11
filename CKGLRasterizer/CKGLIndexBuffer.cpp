#include "CKGLRasterizer.h"

CKGLIndexBufferDesc::CKGLIndexBufferDesc(CKIndexBufferDesc *DesiredFormat)
{
    GLBuffer = 0;
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount; // Max number of indices this buffer can contain
    this->m_CurrentICount = DesiredFormat->m_CurrentICount; // For dynamic buffers, current number of indices taken in this buffer
}


bool CKGLIndexBufferDesc::operator==(const CKIndexBufferDesc & that) const
{
    return
        this->m_Flags == that.m_Flags &&
        this->m_CurrentICount == that.m_CurrentICount &&
        this->m_MaxIndexCount == that.m_MaxIndexCount;
}

void CKGLIndexBufferDesc::Create()
{
    GLCall(glGenBuffers(1, &GLBuffer));
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLBuffer));
    GLCall(glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, 2 * m_MaxIndexCount, nullptr, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT));
    m_Flags |= CKRST_VB_VALID;
}

void CKGLIndexBufferDesc::Bind()
{
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLBuffer));
}

void *CKGLIndexBufferDesc::Lock(CKDWORD offset, CKDWORD len, bool overwrite)
{
    if (!offset && !len)
    {
        //GLCall(glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, (GLint*)&len));
        auto ret = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
        GLLogCall("glMapBuffer", __FILE__, __LINE__);
        return ret;
    }
    auto ret = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, len, GL_MAP_WRITE_BIT | (overwrite ? GL_MAP_INVALIDATE_RANGE_BIT : 0));
    GLLogCall("glMapBufferRange", __FILE__, __LINE__);
    return ret;
}

void CKGLIndexBufferDesc::Unlock()
{
    int locked = 0;
    GLCall(glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_MAPPED, &locked));
    if (!locked) return;
    GLCall(glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER));
}