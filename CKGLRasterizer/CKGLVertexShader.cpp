#include "CKGLRasterizer.h"

bool CKGLVertexShaderDesc::operator==(const CKVertexShaderDesc & that) const
{
    return this->m_Function == that.m_Function &&
        this->m_FunctionSize == that.m_FunctionSize;
}

CKDWORD CKGLVertexShaderDesc::Create(CKGLRasterizerContext *Ctx, CKVertexShaderDesc *Format)
{
    this->Owner = Ctx;

    // We re-purpose this member to hold a pointer to shader source.
    this->m_Function = Format->m_Function;
    this->m_FunctionSize = Format->m_FunctionSize;
    this->GLShader = glCreateShader(GL_VERTEX_SHADER);
    GLCall(glShaderSource(GLShader, 1, (const char**)&m_Function, (const GLint*)&m_FunctionSize));
    GLCall(glCompileShader(GLShader));

    int result;
    GLCall(glGetShaderiv(GLShader, GL_COMPILE_STATUS, &result));
    if (result == GL_FALSE)
    {
        int length;
        GLCall(glGetShaderiv(GLShader, GL_INFO_LOG_LENGTH, &length));
        std::string msg(length, '\0');
        GLCall(glGetShaderInfoLog(GLShader, length, &length, msg.data()));
        MessageBoxA(NULL, msg.c_str(), "Vertex shader", NULL);
        GLCall(glDeleteShader(GLShader));
    }
    return GLShader;
}

CKGLVertexShaderDesc::~CKGLVertexShaderDesc()
{
    GLCall(glDeleteShader(GLShader));
    GLShader = 0;
}
