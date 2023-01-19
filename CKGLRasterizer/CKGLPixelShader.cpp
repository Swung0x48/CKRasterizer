#include "CKGLRasterizer.h"

bool CKGLPixelShaderDesc::operator==(const CKPixelShaderDesc & that) const
{
    return this->m_Function == that.m_Function &&
        this->m_FunctionSize == that.m_FunctionSize;
}

CKDWORD CKGLPixelShaderDesc::Create(CKGLRasterizerContext *Ctx, CKPixelShaderDesc *Format)
{
    this->Owner = Ctx;

    // We re-purpose this member to hold a pointer to shader source.
    this->m_Function = Format->m_Function;
    this->m_FunctionSize = Format->m_FunctionSize;
    this->GLShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(GLShader, 1, (const char**)&m_Function, (const GLint*)&m_FunctionSize);
    glCompileShader(GLShader);

    int result;
    glGetShaderiv(GLShader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        int length;
        glGetShaderiv(GLShader, GL_INFO_LOG_LENGTH, &length);
        std::string msg(length, '\0');
        glGetShaderInfoLog(GLShader, length, &length, (char*)msg.data());
        fprintf(stderr, "fragment shader compilation error: %s\n", msg.c_str());
        glDeleteShader(GLShader);
    }
    return GLShader;
}

CKGLPixelShaderDesc::~CKGLPixelShaderDesc()
{
    glDeleteShader(GLShader);
    GLShader = 0;
}

