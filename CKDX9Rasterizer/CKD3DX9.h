#ifndef CKD3DX9_H
#define CKD3DX9_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <limits.h>

#include <d3d9.h>

#define D3DXASM_DEBUG 0x0001
#define D3DXASM_SKIPVALIDATION 0x0010

#ifdef NDEBUG
#define D3DXASM_FLAGS 0
#else
#define D3DXASM_FLAGS D3DXASM_DEBUG
#endif // NDEBUG

#define D3DX_DEFAULT            ((UINT) -1)
#define D3DX_DEFAULT_NONPOW2    ((UINT) -2)
#define D3DX_DEFAULT_FLOAT      FLT_MAX
#define D3DX_FROM_FILE          ((UINT) -3)
#define D3DFMT_FROM_FILE        ((D3DFORMAT) -3)

#define D3DX_FILTER_NONE (1 << 0)
#define D3DX_FILTER_POINT (2 << 0)
#define D3DX_FILTER_LINEAR (3 << 0)
#define D3DX_FILTER_TRIANGLE (4 << 0)
#define D3DX_FILTER_BOX (5 << 0)

#define D3DX_FILTER_MIRROR_U (1 << 16)
#define D3DX_FILTER_MIRROR_V (2 << 16)
#define D3DX_FILTER_MIRROR_W (4 << 16)
#define D3DX_FILTER_MIRROR (7 << 16)

#define D3DX_FILTER_DITHER (1 << 19)
#define D3DX_FILTER_DITHER_DIFFUSION (2 << 19)

#define D3DX_FILTER_SRGB_IN (1 << 21)
#define D3DX_FILTER_SRGB_OUT (2 << 21)
#define D3DX_FILTER_SRGB (3 << 21)

enum D3DXIMAGE_FILEFORMAT
{
    D3DXIFF_BMP = 0,
    D3DXIFF_JPG = 1,
    D3DXIFF_TGA = 2,
    D3DXIFF_PNG = 3,
    D3DXIFF_DDS = 4,
    D3DXIFF_PPM = 5,
    D3DXIFF_DIB = 6,
    D3DXIFF_HDR = 7, // high dynamic range formats
    D3DXIFF_PFM = 8, //
    D3DXIFF_FORCE_DWORD = 0x7fffffff

};

struct D3DXMACRO
{
    LPCSTR Name;
    LPCSTR Definition;
};

struct D3DXIMAGE_INFO
{
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    D3DFORMAT Format;
    D3DRESOURCETYPE ResourceType;
    D3DXIMAGE_FILEFORMAT ImageFileFormat;
};

typedef interface ID3DXBuffer *LPD3DXBUFFER;
typedef interface ID3DXInclude *LPD3DXINCLUDE;

DECLARE_INTERFACE_(ID3DXBuffer, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID * ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DXBuffer
    STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};

typedef HRESULT(WINAPI *PFN_D3DXAssembleShader)(LPCSTR pSrcData, UINT SrcDataLen, const D3DXMACRO *pDefines,
                                                LPD3DXINCLUDE pInclude, DWORD Flags, LPD3DXBUFFER *ppShader,
                                                LPD3DXBUFFER *ppErrorMsgs);
typedef HRESULT(WINAPI *PFN_D3DXDisassembleShader)(const DWORD *pShader, BOOL EnableColorCode, LPCSTR pComments,
                                                   LPD3DXBUFFER *ppDisassembly);
typedef HRESULT(WINAPI *PFN_D3DXLoadSurfaceFromSurface)(LPDIRECT3DSURFACE9 pDestSurface,
                                                        const PALETTEENTRY *pDestPalette, const RECT *pDestRect,
                                                        LPDIRECT3DSURFACE9 pSrcSurface, const PALETTEENTRY *pSrcPalette,
                                                        const RECT *pSrcRect, DWORD Filter, D3DCOLOR ColorKey);
typedef HRESULT(WINAPI *PFN_D3DXLoadSurfaceFromMemory)(LPDIRECT3DSURFACE9 pDestSurface,
                                                       CONST PALETTEENTRY *pDestPalette, CONST RECT *pDestRect,
                                                       LPCVOID pSrcMemory, D3DFORMAT SrcFormat, UINT SrcPitch,
                                                       CONST PALETTEENTRY *pSrcPalette, CONST RECT *pSrcRect,
                                                       DWORD Filter, D3DCOLOR ColorKey);
typedef HRESULT(WINAPI *PFN_D3DXCreateTextureFromFileExA)(LPDIRECT3DDEVICE9 pDevice, LPCSTR pSrcFile, UINT Width,
                                                          UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format,
                                                          D3DPOOL Pool, DWORD Filter, DWORD MipFilter,
                                                          D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo,
                                                          PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture);

extern PFN_D3DXAssembleShader D3DXAssembleShader;
extern PFN_D3DXDisassembleShader D3DXDisassembleShader;
extern PFN_D3DXLoadSurfaceFromSurface D3DXLoadSurfaceFromSurface;
extern PFN_D3DXLoadSurfaceFromMemory D3DXLoadSurfaceFromMemory;
extern PFN_D3DXCreateTextureFromFileExA D3DXCreateTextureFromFileExA;

#endif