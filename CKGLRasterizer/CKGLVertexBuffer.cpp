#include "CKGLRasterizer.h"

CKGLVertexBufferDesc::CKGLVertexBufferDesc(CKVertexBufferDesc* DesiredFormat)
{
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_VertexFormat = DesiredFormat->m_VertexFormat;   // Vertex format : CKRST_VERTEXFORMAT
    this->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount; // Max number of vertices this buffer can contain
    this->m_VertexSize = DesiredFormat->m_VertexSize;     // Size in bytes taken by a vertex..
    this->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    this->GLLayout = GLVertexBufferLayout::GetLayoutFromFVF(DesiredFormat->m_VertexFormat);
}

bool CKGLVertexBufferDesc::operator==(const CKVertexBufferDesc & that) const
{
    return this->m_VertexSize == that.m_VertexSize &&
        this->m_VertexFormat == that.m_VertexFormat &&
        this->m_CurrentVCount == that.m_CurrentVCount &&
        this->m_Flags == that.m_Flags;
}

void CKGLVertexBufferDesc::Create()
{
    GLCall(glGenBuffers(1, &GLBuffer));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
    GLCall(glNamedBufferStorage(GLBuffer, this->m_MaxVertexCount * this->m_VertexSize, NULL, GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT));
    GLCall(glGenVertexArrays(1, &GLVertexArray));
    GLCall(glBindVertexArray(GLVertexArray));
    const auto& elements = GLLayout.GetElements();
    unsigned int offset = 0;
    for (unsigned int i = 0; i < elements.size(); ++i)
    {
        const auto& element = elements[i];
        GLCall(glVertexAttribPointer(element.index, element.count,
            element.type, element.normalized, GLLayout.GetStride(), (const GLvoid*)offset));
        GLCall(glEnableVertexAttribArray(element.index));
        offset += element.count * GLVertexBufferElement::GetSizeOfType(element.type);
    }
}

void CKGLVertexBufferDesc::Bind(CKGLRasterizerContext *ctx)
{
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
    GLCall(glBindVertexArray(GLVertexArray));
    ctx->set_position_transformed(GLLayout.GetElements().front().usage == CKRST_VF_RASTERPOS);
    ctx->set_vertex_has_color(m_VertexFormat & CKRST_VF_DIFFUSE);
}

void *CKGLVertexBufferDesc::Lock(CKDWORD offset, CKDWORD len, bool overwrite)
{
    if (!offset && !len)
    {
        GLCall(glGetNamedBufferParameteriv(GLBuffer, GL_BUFFER_SIZE, (GLint*)&len));
    }
    auto ret = glMapNamedBufferRange(GLBuffer, offset, len, GL_MAP_WRITE_BIT | (overwrite ? GL_MAP_INVALIDATE_RANGE_BIT : 0));
    GLLogCall("glMapNamedBufferRange", __FILE__, __LINE__);
    return ret;
}

void CKGLVertexBufferDesc::Unlock()
{
    int locked = 0;
    GLCall(glGetNamedBufferParameteriv(GLBuffer, GL_BUFFER_MAPPED, &locked));
    if (!locked) return;
    GLCall(glUnmapNamedBuffer(GLBuffer));
}
