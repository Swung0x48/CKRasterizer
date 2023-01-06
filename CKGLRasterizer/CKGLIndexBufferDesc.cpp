#include "CKGLRasterizer.h"

CKGLIndexBufferDesc::CKGLIndexBufferDesc(CKIndexBufferDesc *DesiredFormat)
{
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

void CKGLIndexBufferDesc::Populate(GLushort* data, GLsizei count)
{
    GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(GLushort), data, GL_STATIC_DRAW));
}

void CKGLIndexBufferDesc::Create()
{
    GLCall(glGenBuffers(1, &GLBuffer));
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLBuffer));
    /*GLCall(glBufferData(GL_ARRAY_BUFFER,
        2 * m_MaxIndexCount, 
        nullptr, GL_STATIC_DRAW));*/
}

void CKGLIndexBufferDesc::Bind()
{
    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLBuffer));
}