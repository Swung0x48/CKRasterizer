#ifndef CKRASTERIZER_CKDX11RASTERIZERCOMMON_H
#define CKRASTERIZER_CKDX11RASTERIZERCOMMON_H

#include "tracy/Tracy.hpp"
#include <string>
#include <format>

#ifdef _DEBUG
    #define D3DCall(x) {\
        x;\
        D3DLogCall(hr, #x, __FILE__, __LINE__);}
#else
    #define D3DCall(x) x;
#endif

bool D3DLogCall(HRESULT hr, const char* function, const char* file, int line);

#endif // CKRASTERIZER_CKDX11RASTERIZERCOMMON_H
