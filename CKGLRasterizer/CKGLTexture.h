#ifndef CKGLTEXTURE_H
#define CKGLTEXTURE_H

#include <CKRasterizerTypes.h>

#include <GL/glew.h>
#include <gl/GL.h>

#include "CKGLRasterizerCommon.h"

#include <unordered_map>

typedef struct CKGLTexture : public CKTextureDesc
{
private:
    GLuint tex;
    GLenum glfmt;
    GLenum gltyp;
    std::unordered_map<GLenum, int> params;
public:
    CKGLTexture() { tex = 0; glfmt = gltyp = GL_INVALID_ENUM; }
    CKGLTexture(CKTextureDesc *texdesc);
    ~CKGLTexture() { glDeleteTextures(1, &tex); }

    void set_parameter(GLenum p, int pv);
    void set_border_color(int color);
    void Create();
    void Bind();
    void Load(void *data);
} CKGLTexture;

#endif