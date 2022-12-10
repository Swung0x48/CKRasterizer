#pragma once
#include "common_include.h"
typedef struct CKDX9VertexBufferDesc : public CKVertexBufferDesc
{
public:
	LPDIRECT3DVERTEXBUFFER9 DxBuffer;
public:
	CKDX9VertexBufferDesc() {
		DxBuffer = NULL;
	}
	~CKDX9VertexBufferDesc() {

#ifdef _XBOX // not tested
		int refcount = 0;
		if (DxBuffer) {
			BYTE* Data = NULL;
			DxBuffer->Lock(0, 0, (LPBYTE*)&Data, 0);
			DxBuffer->Unlock();
			refcount = DxBuffer->Release();
		}
		if (refcount)
			int j = 0;
#else
		SAFERELEASE(DxBuffer);
#endif

	}
} CKDX9VertexBufferDesc;