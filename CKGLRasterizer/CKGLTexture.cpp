#include "CKGLTexture.h"
#include "CKGLRasterizer.h"

#include <VxColor.h>

CKGLTexture::CKGLTexture(CKTextureDesc *texdesc) : CKTextureDesc(*texdesc)
{
    tex = 0;
    glfmt = GL_RGBA;
    gltyp = GL_UNSIGNED_BYTE;
}

void CKGLTexture::set_parameter(GLenum p, int pv)
{
    if (params.find(p) == params.end() || params[p] != pv)
    {
        glTextureParameteri(tex, p, pv);
        params[p] = pv;
    }
}

void CKGLTexture::set_border_color(int color)
{
    if (params.find(GL_TEXTURE_BORDER_COLOR) == params.end() || params[GL_TEXTURE_BORDER_COLOR] != color)
    {
        VxColor c((CKDWORD)color);
        glTextureParameterfv(tex, GL_TEXTURE_BORDER_COLOR, (float*)&c.col);
        params[GL_TEXTURE_BORDER_COLOR] = color;
    }
}

void CKGLTexture::Create()
{
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA8, Format.Width, Format.Height);
    set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void CKGLTexture::Bind()
{
    glBindTexture(GL_TEXTURE_2D, tex);
}

void CKGLTexture::Load(void *data)
{
    glTextureSubImage2D(tex, 0, 0, 0, Format.Width, Format.Height, glfmt, gltyp, data);
}
