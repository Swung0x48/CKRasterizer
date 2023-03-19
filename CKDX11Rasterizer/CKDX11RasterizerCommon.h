#ifndef CKRASTERIZER_CKDX11RASTERIZERCOMMON_H
#define CKRASTERIZER_CKDX11RASTERIZERCOMMON_H

#include "tracy/Tracy.hpp"
#include "tracy/TracyD3D11.hpp"
#include <string>
#include <format>
#include <cstdlib>

#ifdef _DEBUG
    #define D3DCall(x) {\
        hr = x;\
        D3DLogCall(hr, #x, __FILE__, __LINE__);}
#else
    #define D3DCall(x) x;
#endif

#if TRACY_ENABLE
static tracy::D3D11Ctx *g_D3d11Ctx = nullptr;
#endif

bool D3DLogCall(HRESULT hr, const char* function, const char* file, int line);

class CKContext;
extern CKContext *rst_ckctx;

#endif // CKRASTERIZER_CKDX11RASTERIZERCOMMON_H
