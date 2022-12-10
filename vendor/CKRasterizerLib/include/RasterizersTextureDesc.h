#ifndef RASTERIZERSTEXTUREDESC_H
#define RASTERIZERSTEXTUREDESC_H

#include "VxDefines.h"
#include "CKTypes.h"
#include "XArray.h"
#include "VxMemoryPool.h"

#ifdef DX8
/***********************************************************
DIRECTX 8.1
************************************************************/
/*******************************************
 A texture object for DX8 : contains the
 texture surface pointer
*********************************************/
typedef struct CKDX8TextureDesc : public CKTextureDesc
{
public:
    union
    {
        LPDIRECT3DTEXTURE8 DxTexture;
        LPDIRECT3DCUBETEXTURE8 DxCubeTexture;
    };
    //----- For render target textures...
    LPDIRECT3DTEXTURE8 DxRenderTexture;
    //----- For non managed surface to be locked (DX8 does not support direct locking anymore)
    LPDIRECT3DSURFACE8 DxLockedSurface;
    CKDWORD LockedFlags;

public:
    CKDX8TextureDesc()
    {
        DxTexture = NULL;
        DxRenderTexture = NULL;
        DxLockedSurface = NULL;
    }
    ~CKDX8TextureDesc()
    {
        SAFERELEASE(DxTexture);
        SAFERELEASE(DxRenderTexture);
        SAFERELEASE(DxLockedSurface);
    }
} CKDX8TextureDesc;
#endif

#ifdef DX7
/***********************************************************
DIRECTX 7
************************************************************/

/*******************************************
 A texture object for DX7 : contains the
 texture surface pointer
*********************************************/
typedef struct CKDX7TextureDesc : public CKTextureDesc
{
public:
    LPDIRECTDRAWSURFACE7 DxSurface;		  //
    LPDIRECTDRAWSURFACE7 DxRenderSurface; // If texture is used to be rendered too...
    CKDX7TextureDesc()
    {
        DxSurface = NULL;
        DxRenderSurface = NULL;
    }
    ~CKDX7TextureDesc()
    {
        DetachZBuffer();
        SAFERELEASE(DxSurface);
        SAFERELEASE(DxRenderSurface);
    }
    void DetachZBuffer()
    {
        if (DxSurface)
            DxSurface->DeleteAttachedSurface(0, NULL);
        if (DxRenderSurface)
            DxRenderSurface->DeleteAttachedSurface(0, NULL);
    }
} CKDX7TextureDesc;
#endif

#ifdef DX5
/***********************************************************
DIRECTX 5
************************************************************/

/*************************************************
 Overload of the CKTextureDesc struct for DirectX 5
**************************************************/
typedef struct CKDX5TextureDesc : public CKTextureDesc
{
public:
    LPDIRECTDRAWSURFACE3 DxSurface;		  // Direct Draw Surface
    LPDIRECTDRAWSURFACE3 DxRenderSurface; // Direct Draw Surface when rendering to texture
    D3DTEXTUREHANDLE DxHandle;			  // D3D texture handle
    D3DTEXTUREHANDLE DxRenderHandle;
    CKDX5TextureDesc()
    {
        DxHandle = 0;
        DxSurface = NULL;
        DxRenderHandle = 0;
        DxRenderSurface = NULL;
    }
    ~CKDX5TextureDesc()
    {
        if (DxRenderSurface)
        {
            // In case a Z-buffer was attached to these surfaces...
            DxSurface->DeleteAttachedSurface(0, NULL);
            DxRenderSurface->DeleteAttachedSurface(0, NULL);
        }
        SAFERELEASE(DxSurface);
        SAFERELEASE(DxRenderSurface);
    }
} CKDX5TextureDesc;
#endif

#ifdef OPENGL
/***********************************************************
OPENGL
************************************************************/

//---------------------------------------------
// Texture parameter as set by glTexParameter()
// that are unique to each texture object
// need to keep track of these as they may invalidate the
// texture stage states cache
typedef struct CKGLTexParameter
{
    CKDWORD BorderColor;
    CKDWORD MinFilter;
    CKDWORD MagFilter;
    CKDWORD AddressU;
    CKDWORD AddressV;
    CKGLTexParameter()
    {
        MinFilter = MagFilter = AddressU = AddressV = 0xFFFFFFFF;
        AddressU = AddressV = 0;
    }
} CKGLTexParameter;

//---------------------------------------------
// Video memory of a texture can not be locked directly
// instead we can copy it to a system memory surface
// and return the pointer to the user...
typedef struct CKGLTexLockData
{
    VxImageDescEx Image;
    VxMemoryPool ImageMemory;
    CKDWORD LockFlags;
} CKGLTexLockData;

/*******************************************
 A texture object for OpenGL : contains a cache
 for its attribute (Blend,Filter,etc...) and
 the index the texture is binded to...
*********************************************/
typedef struct CKGLTextureDesc : public CKTextureDesc
{
public:
    CKGLTexParameter TexParam;
    int GlTextureIndex;
    CKGLTexLockData *LockData;

public:
    CKGLTextureDesc()
    {
        LockData = NULL;
        GlTextureIndex = -1;
    }
    ~CKGLTextureDesc() { GlTextureIndex = -1; }
} CKGLTextureDesc;
#endif

#endif // RASTERIZERSTEXTUREDESC_H