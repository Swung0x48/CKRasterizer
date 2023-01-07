#include "CKGLRasterizer.h"

CKGLTextureDesc::CKGLTextureDesc(CKTextureDesc *texdesc) : CKTextureDesc(*texdesc)
{
    tex = 0;
    glfmt = GL_RGBA;
    gltyp = GL_UNSIGNED_BYTE;
}

void CKGLTextureDesc::Create()
{
    GLCall(glGenTextures(1, &tex));
    GLCall(glBindTexture(GL_TEXTURE_2D, tex));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
}

void CKGLTextureDesc::Bind(CKGLRasterizerContext *ctx)
{
    GLCall(glBindTexture(GL_TEXTURE_2D, tex));
}

void CKGLTextureDesc::Load(void *data)
{
    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Format.Width, Format.Height, 0, glfmt, gltyp, data));
}
