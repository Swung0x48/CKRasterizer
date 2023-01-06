#include "CKGLRasterizer.h"

bool CKGLVertexBufferDesc::operator==(const CKVertexBufferDesc & that) const
{
    return this->m_VertexSize == that.m_VertexSize &&
        this->m_VertexFormat == that.m_VertexFormat &&
        this->m_CurrentVCount == that.m_CurrentVCount &&
        this->m_Flags == that.m_Flags;
}

void CKGLVertexBufferDesc::Populate(CKVertexBufferDesc *DesiredFormat)
{
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_VertexFormat = DesiredFormat->m_VertexFormat;   // Vertex format : CKRST_VERTEXFORMAT
    this->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount; // Max number of vertices this buffer can contain
    this->m_VertexSize = DesiredFormat->m_VertexSize;     // Size in bytes taken by a vertex..
    this->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    this->GLLayout = GLVertexBufferLayout::GetLayoutFromFVF(DesiredFormat->m_VertexFormat);
}

void CKGLVertexBufferDesc::Create()
{
    GLCall(glGenBuffers(1, &GLBuffer));
    //glBindBuffer(GL_ARRAY_BUFFER, GLBuffer);
    /*glBufferData(GL_ARRAY_BUFFER,
        m_MaxVertexCount * m_VertexSize, 
        nullptr, GL_STATIC_DRAW);*/
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
    GLCall(glGenVertexArrays(1, &GLVertexArray));
    GLCall(glBindVertexArray(GLVertexArray));
    const auto& elements = GLLayout.GetElements();
    unsigned int offset = 0;
    for (unsigned int i = 0; i < elements.size(); ++i)
    {
        const auto& element = elements[i];
        GLCall(glVertexAttribPointer(i, element.count, 
            element.type, element.normalized, GLLayout.GetStride(), (const GLvoid*)offset));
        GLCall(glEnableVertexAttribArray(i));
        offset += element.count * GLVertexBufferElement::GetSizeOfType(element.type);
    }
}

void CKGLVertexBufferDesc::Bind()
{
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
    GLCall(glBindVertexArray(GLVertexArray));
}