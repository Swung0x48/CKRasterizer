#ifndef CKGLINDEXBUFFER_H
#define CKGLINDEXBUFFER_H

#include <CKRasterizerTypes.h>
#include <CKRasterizerEnums.h>

#include <GL/glew.h>
#include <gl/GL.h>

#include "CKGLRasterizerCommon.h"

struct CKGLIndexBuffer : public CKIndexBufferDesc
{
public:
    GLuint GLBuffer;
    void *client_side_data = nullptr;
    void *client_side_locked_data = nullptr;
    CKDWORD lock_offset;
    CKDWORD lock_length;
public:
    bool operator==(const CKIndexBufferDesc &) const;
    void Create();
    void *Lock(CKDWORD offset, CKDWORD len, bool overwrite);
    void Unlock();
    void Bind();
    explicit CKGLIndexBuffer(CKIndexBufferDesc* DesiredFormat);
    ~CKGLIndexBuffer();
};

#endif