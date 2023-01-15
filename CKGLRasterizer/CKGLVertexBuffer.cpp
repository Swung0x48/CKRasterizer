#include "CKGLVertexBuffer.h"

#include "CKGLRasterizer.h"

CKGLVertexBuffer::CKGLVertexBuffer(CKVertexBufferDesc* DesiredFormat)
{
    GLBuffer = 0;
    this->m_Flags = DesiredFormat->m_Flags;          // CKRST_VBFLAGS
    this->m_VertexFormat = DesiredFormat->m_VertexFormat;   // Vertex format : CKRST_VERTEXFORMAT
    this->m_MaxVertexCount = DesiredFormat->m_MaxVertexCount; // Max number of vertices this buffer can contain
    this->m_VertexSize = DesiredFormat->m_VertexSize;     // Size in bytes taken by a vertex..
    this->m_CurrentVCount = DesiredFormat->m_CurrentVCount;
    lock_offset = 0;
    lock_length = 0;
#if !USE_SEPARATE_ATTRIBUTE
    GLVertexArray = 0;
    this->GLLayout = GLVertexBufferLayout::GetLayoutFromFVF(DesiredFormat->m_VertexFormat);
#endif
}
CKGLVertexBuffer::~CKGLVertexBuffer()
{
    GLCall(glDeleteBuffers(1, &GLBuffer));
    GLBuffer  = 0;
#if !USE_SEPARATE_ATTRIBUTE
    GLCall(glDeleteVertexArrays(1, &GLVertexArray));
#endif
    if (client_side_data)
        VirtualFree(client_side_data, 0, MEM_RELEASE);
    client_side_data = nullptr;
    if (client_side_locked_data)
        VirtualFree(client_side_locked_data, 0, MEM_RELEASE);
    client_side_locked_data = nullptr;
}

bool CKGLVertexBuffer::operator==(const CKVertexBufferDesc & that) const
{
    return this->m_VertexSize == that.m_VertexSize &&
        this->m_VertexFormat == that.m_VertexFormat &&
        this->m_CurrentVCount == that.m_CurrentVCount &&
        this->m_Flags == that.m_Flags;
}

void CKGLVertexBuffer::Create()
{
    GLCall(glGenBuffers(1, &GLBuffer));
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
    GLenum flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
    if (!(m_Flags & CKRST_VB_WRITEONLY)) //virtools header says this bit is always set, but just in case...
        flags |= GL_MAP_READ_BIT;
    if (!(m_Flags & CKRST_VB_DYNAMIC))
    {
        client_side_data = VirtualAlloc(nullptr, m_VertexSize * m_MaxVertexCount, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    }
    else
        GLCall(glNamedBufferStorage(GLBuffer, this->m_MaxVertexCount * this->m_VertexSize, NULL, flags));
#if !USE_SEPARATE_ATTRIBUTE
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
#endif
    m_Flags |= CKRST_VB_VALID;
}

void CKGLVertexBuffer::Bind(CKGLRasterizerContext *ctx)
{
    ZoneScopedN(__FUNCTION__);
    GLCall(glBindBuffer(GL_ARRAY_BUFFER, GLBuffer));
#if !USE_SEPARATE_ATTRIBUTE
    GLCall(glBindVertexArray(GLVertexArray));
    ctx->set_position_transformed(m_VertexFormat & CKRST_VF_RASTERPOS);
    ctx->set_vertex_has_color(m_VertexFormat & CKRST_VF_DIFFUSE);
    ctx->set_num_textures((m_VertexFormat & CKRST_VF_TEXMASK) >> 8);
#endif
}

void CKGLVertexBuffer::bind_to_array()
{
    GLCall(glBindVertexBuffer(0, GLBuffer, 0, m_VertexSize));
}

void *CKGLVertexBuffer::Lock(CKDWORD offset, CKDWORD len, bool overwrite)
{
    ZoneScopedN(__FUNCTION__);
    if (!offset && !len)
    {
        len = m_MaxVertexCount * m_VertexSize;
    }
    if (client_side_data)
    {
        lock_offset = offset;
        lock_length = len;
        if (offset == 0 && len == m_MaxVertexCount * m_VertexSize)
            return client_side_data;
        client_side_locked_data = VirtualAlloc(nullptr, len, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
        return client_side_locked_data;
    }
    void* ret = nullptr;
    {
        TracyGpuZone(GLZoneName(x));
        ret = glMapNamedBufferRange(GLBuffer, offset, len, GL_MAP_WRITE_BIT | (overwrite ? GL_MAP_INVALIDATE_RANGE_BIT : 0));
        GLLogCall("glMapNamedBufferRange", __FILE__, __LINE__);
    }
    return ret;
}

void CKGLVertexBuffer::Unlock()
{
    if (client_side_data)
    {
        size_t x[8], c = 8; DWORD _g;
        if (!client_side_locked_data) //the entire buffer locked
        {
            GetWriteWatch(WRITE_WATCH_FLAG_RESET, client_side_data, lock_length, (void**)&x, (ULONG_PTR*)&c, &_g);
            if (c > 0)
                GLCall(glNamedBufferData(GLBuffer, m_VertexSize * m_MaxVertexCount, client_side_data, GL_STATIC_DRAW));
        }
        else
        {
            GetWriteWatch(WRITE_WATCH_FLAG_RESET, client_side_locked_data, lock_length, (void**)&x, (ULONG_PTR*)&c, &_g);
            if (c > 0)
            {
                memcpy((uint8_t*)client_side_data + lock_offset, client_side_locked_data, lock_length);
                GLCall(glNamedBufferData(GLBuffer, m_VertexSize * m_MaxVertexCount, client_side_data, GL_STATIC_DRAW));
            }
            VirtualFree(client_side_locked_data, 0, MEM_RELEASE);
            client_side_locked_data = nullptr;
        }
        lock_offset = ~0U;
        lock_length = 0;
        return;
    }
    int locked = 0;
    GLCall(glGetNamedBufferParameteriv(GLBuffer, GL_BUFFER_MAPPED, &locked));
    if (!locked) return;
    GLCall(glUnmapNamedBuffer(GLBuffer));
}

CKGLVertexFormat::CKGLVertexFormat(CKRST_VERTEXFORMAT vf) : ckvf(vf)
{
    GLCall(glGenVertexArrays(1, &GLVertexArray));
    GLCall(glBindVertexArray(GLVertexArray));
    auto GLLayout = GLVertexBufferLayout::GetLayoutFromFVF(vf);
    const auto& elements = GLLayout.GetElements();
    unsigned int offset = 0;
    for (unsigned int i = 0; i < elements.size(); ++i)
    {
        const auto& element = elements[i];
        GLCall(glVertexAttribFormat(element.index, element.count,
            element.type, element.normalized, offset));
        GLCall(glVertexAttribBinding(element.index, 0));
        GLCall(glEnableVertexAttribArray(element.index));
        offset += element.count * GLVertexBufferElement::GetSizeOfType(element.type);
    }
}

CKGLVertexFormat::~CKGLVertexFormat()
{
    GLCall(glDeleteVertexArrays(1, &GLVertexArray));
}

void CKGLVertexFormat::select(CKGLRasterizerContext *ctx)
{
    GLCall(glBindVertexArray(GLVertexArray));
    ctx->set_position_transformed(ckvf & CKRST_VF_RASTERPOS);
    ctx->set_vertex_has_color(ckvf & CKRST_VF_DIFFUSE);
    ctx->set_num_textures((ckvf & CKRST_VF_TEXMASK) >> 8);
}
