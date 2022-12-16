#include "CKDX9Rasterizer.h"

BOOL CKDX9PixelShaderDesc::Create(CKDX9RasterizerContext *Ctx, CKDWORD *Function)
{
    Owner = Ctx;
    m_Function = Function;
    return SUCCEEDED(Ctx->m_Device->SetPixelShader(DxShader));
}

CKDX9PixelShaderDesc::~CKDX9PixelShaderDesc() = default;
