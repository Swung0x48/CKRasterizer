#include "CKGLRasterizer.h"

bool CKGLIndexBufferDesc::operator==(const CKIndexBufferDesc & that) const
{
    return
        this->m_Flags == that.m_Flags &&
        this->m_CurrentICount == that.m_CurrentICount &&
        this->m_MaxIndexCount == that.m_MaxIndexCount;
}

void CKGLIndexBufferDesc::Populate(CKIndexBufferDesc *DesiredFormat)
{
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_MaxIndexCount = DesiredFormat->m_MaxIndexCount; // Max number of indices this buffer can contain
    this->m_CurrentICount = DesiredFormat->m_CurrentICount; // For dynamic buffers, current number of indices taken in this buffer
}

void CKGLIndexBufferDesc::Create()
{
    glGenBuffers(1, &GLBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, GLBuffer);
    glBufferData(GL_ARRAY_BUFFER,
        2 * m_MaxIndexCount, 
        nullptr, GL_STATIC_DRAW);
}

void CKGLIndexBufferDesc::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, GLBuffer);
}