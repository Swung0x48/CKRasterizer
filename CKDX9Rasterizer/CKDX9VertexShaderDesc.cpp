#include "CKDX9Rasterizer.h"

CKBOOL CKDX9VertexShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKVertexShaderDesc *Format)
{
    Owner = Ctx;
    m_FunctionSize = Format->m_FunctionSize;
    if (m_FunctionSize >= m_FunctionData.Size())
        m_FunctionData.Resize(m_FunctionSize);
    memcpy(&m_FunctionData[0], Format->m_Function, Format->m_FunctionSize);
    return 1;
}

CKDX9VertexShaderDesc::~CKDX9VertexShaderDesc() = default;
