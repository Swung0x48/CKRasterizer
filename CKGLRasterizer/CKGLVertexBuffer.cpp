#include "CKGLRasterizer.h"

void CKGLVertexBufferDesc::Populate(CKVertexBufferDesc* DesiredFormat)
{
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_VertexFormat = DesiredFormat->m_VertexFormat;   // Vertex format : CKRST_VERTEXFORMAT
    this->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount; // Max number of vertices this buffer can contain
    this->m_VertexSize = DesiredFormat->m_VertexSize;     // Size in bytes taken by a vertex..
    this->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
}

void CKGLVertexBufferDesc::Create()
{
    glGenBuffers(1, &GLBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, GLBuffer);
    glBufferData(GL_ARRAY_BUFFER,
        m_MaxVertexCount * m_VertexSize, 
        nullptr, GL_STATIC_DRAW);
}

void CKGLVertexBufferDesc::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, GLBuffer);
}