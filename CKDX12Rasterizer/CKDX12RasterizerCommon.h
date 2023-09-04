#ifndef CKRASTERIZER_CKDX12RASTERIZERCOMMON_H
#define CKRASTERIZER_CKDX12RASTERIZERCOMMON_H

#include <cstdlib>
#include <format>
#include <string>

#include <intsafe.h>

#include "asio.hpp"
#include <CKGlobals.h>

#ifdef _DEBUG
#define D3DCall(x)                                                                                                     \
    {                                                                                                                  \
        hr = x;                                                                                                        \
        D3DLogCall(hr, #x, __FILE__, __LINE__);                                                                        \
    }
#else
#define D3DCall(x) x;
#endif


bool D3DLogCall(HRESULT hr, const char *function, const char *file, int line);


#endif // CKRASTERIZER_CKDX12RASTERIZERCOMMON_H
