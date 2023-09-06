#ifndef CKRASTERIZER_CKDX12RASTERIZERCOMMON_H
#define CKRASTERIZER_CKDX12RASTERIZERCOMMON_H

#include <cstdlib>
#include <format>
#include <string>

#include <intsafe.h>

#include "asio.hpp"
#include <CKGlobals.h>

#include <tracy/Tracy.hpp>
#include <tracy/TracyD3D12.hpp>

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

#if TRACY_ENABLE
static tracy::D3D12QueueCtx *g_D3d12Ctx = nullptr;
#endif


#endif // CKRASTERIZER_CKDX12RASTERIZERCOMMON_H
