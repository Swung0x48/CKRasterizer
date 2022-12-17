#pragma once
#include <Windows.h>
#include <CKRasterizer.h>
#ifdef _DEBUG
    #define D3D_DEBUG_INFO
#endif
#include <d3d9.h>
#include <d3dx9.h>
#include <CKVertexBuffer.h>

#define PREPAREDXSTRUCT(x) {  memset(&x, 0, sizeof(x));  x.dwSize = sizeof(x); }
#define SAFERELEASE(x) { if (x) x->Release(); x = NULL; } 
#define SAFELASTRELEASE(x) { int refCount= 0; if (x)  refCount= x->Release(); x = NULL;  XASSERT(refCount == 0); } 
