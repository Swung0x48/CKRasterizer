#include "CKGLRasterizer.h"

CKGLTextureDesc::CKGLTextureDesc(CKTextureDesc *texdesc) : CKTextureDesc(*texdesc)
{
    tex = 0;
    glfmt = GL_RGBA;
    gltyp = GL_UNSIGNED_BYTE;
}

void CKGLTextureDesc::Create()
{
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void CKGLTextureDesc::Bind(CKGLRasterizerContext *ctx)
{
    glBindTexture(GL_TEXTURE_2D, tex);
}

void CKGLTextureDesc::Load(void *data)
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Format.Width, Format.Height, 0, glfmt, gltyp, data);
}
