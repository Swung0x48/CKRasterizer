#ifndef CKGLVERTEXBUFFER_H
#define CKGLVERTEXBUFFER_H

#include <vector>

#include <CKRasterizerTypes.h>
#include <CKRasterizerEnums.h>

#include <GL/glew.h>
#include <gl/GL.h>

#include "CKGLRasterizerCommon.h"

#define USE_SEPARATE_ATTRIBUTE 1 //requires OpenGL 4.3

class CKGLRasterizerContext;

typedef struct GLVertexBufferElement {
    GLuint index = ~0U;
    GLenum type = GL_NONE;
    unsigned int count = 0;
    GLboolean normalized = GL_FALSE;
    CKDWORD usage = 0;

    static unsigned int GetSizeOfType(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT: return 4;
            case GL_UNSIGNED_INT: return 4;
            case GL_UNSIGNED_BYTE: return 1;
            default: break;
        }
        assert(false);
        return 0;
    }
} GLVertexBufferElement;

class GLVertexBufferLayout
{
public:
    GLVertexBufferLayout() : stride_(0)
    {
    }
    template<typename T>
    void push(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        static_assert(sizeof(T) == 0, "pushing this type haven't been implemented.");
    }

    template<>
    void push<GLfloat>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_.push_back({ index, GL_FLOAT, count, normalized, usage });
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_FLOAT) * count;
    }

    template<>
    void push<GLuint>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_.push_back({ index, GL_UNSIGNED_INT, count, normalized, usage });
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_UNSIGNED_INT) * count;
    }

    template<>
    void push<GLubyte>(unsigned int index, unsigned int count, GLboolean normalized, CKDWORD usage)
    {
        elements_.push_back({ index, GL_UNSIGNED_BYTE, count, normalized, usage });
        stride_ += GLVertexBufferElement::GetSizeOfType(GL_UNSIGNED_BYTE) * count;
    }
    inline const auto& GetElements() const { return elements_; }
    inline unsigned int GetStride() const { return stride_; }
    static GLVertexBufferLayout GetLayoutFromFVF(CKDWORD fvf)
    {
        GLVertexBufferLayout layout;
        if (fvf & CKRST_VF_POSITION)
            layout.push<GLfloat>(0, 3, GL_FALSE, CKRST_VF_POSITION);

        if (fvf & CKRST_VF_RASTERPOS)
            layout.push<GLfloat>(0, 4, GL_FALSE, CKRST_VF_RASTERPOS);

        if (fvf & CKRST_VF_NORMAL)
            layout.push<GLfloat>(1, 3, GL_FALSE, CKRST_VF_NORMAL);

        if (fvf & CKRST_VF_DIFFUSE)
            layout.push<GLubyte>(2, 4, GL_TRUE, CKRST_VF_DIFFUSE);

        if (fvf & CKRST_VF_SPECULAR)
            layout.push<GLubyte>(3, 4, GL_TRUE, CKRST_VF_SPECULAR);

        if (fvf & CKRST_VF_TEX1)
            layout.push<GLfloat>(4, 2, GL_FALSE, CKRST_VF_TEX1);

        if (fvf & CKRST_VF_TEX2)
        {
            layout.push<GLfloat>(4, 2, GL_FALSE, CKRST_VF_TEX1);
            layout.push<GLfloat>(5, 2, GL_FALSE, CKRST_VF_TEX2);
        }

        return layout;
    }
private:
    std::vector<GLVertexBufferElement> elements_;
    unsigned int stride_;
};

class CKGLVertexFormat
{
private:
    GLuint GLVertexArray;
    CKDWORD ckvf;

public:
    CKGLVertexFormat(CKRST_VERTEXFORMAT vf);
    ~CKGLVertexFormat();

    void select(CKGLRasterizerContext *ctx);
};

struct CKGLVertexBuffer : public CKVertexBufferDesc
{
private:
    GLuint GLBuffer;
#if !USE_SEPARATE_ATTRIBUTE
    GLVertexBufferLayout GLLayout;
    GLuint GLVertexArray;
#endif
    void *client_side_data = nullptr;
    void *client_side_locked_data = nullptr;
    CKDWORD lock_offset;
    CKDWORD lock_length;
public:
    bool operator==(const CKVertexBufferDesc &) const;
    void Create();
    void Bind(CKGLRasterizerContext *ctx);
    void bind_to_array();
    void *Lock(CKDWORD offset, CKDWORD len, bool overwrite);
    void Unlock();
    explicit CKGLVertexBuffer(CKVertexBufferDesc* DesiredFormat);
    ~CKGLVertexBuffer();
};
#endif