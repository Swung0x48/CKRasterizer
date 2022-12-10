#pragma once
#include "common_include.h"
typedef struct CKDX9IndexBufferDesc : public CKIndexBufferDesc
{
public:
	LPDIRECT3DINDEXBUFFER9	DxBuffer;
public:
	CKDX9IndexBufferDesc() { DxBuffer = NULL; }
	~CKDX9IndexBufferDesc() {

#ifdef _XBOX
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
} CKDX9IndexBufferDesc;