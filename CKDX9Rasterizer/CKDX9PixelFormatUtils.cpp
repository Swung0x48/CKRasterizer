#pragma once
#include "CKDX9Rasterizer.h"

D3DFORMAT VxPixelFormatToD3DFormat(VX_PIXELFORMAT pf)
{
	switch (pf)
	{
		case _32_ARGB8888: return D3DFMT_A8R8G8B8; // 32-bit ARGB pixel format with alpha
		case _32_RGB888: return D3DFMT_X8R8G8B8; // 32-bit RGB pixel format without alpha
		case _24_RGB888: return D3DFMT_R8G8B8; // 24-bit RGB pixel format
		case _16_RGB565: return D3DFMT_R5G6B5; // 16-bit RGB pixel format
		case _16_RGB555: return D3DFMT_X1R5G5B5; // 16-bit RGB pixel format (5 bits per color)
		case _16_ARGB1555: return D3DFMT_A1R5G5B5; // 16-bit ARGB pixel format (5 bits per color + 1 bit for alpha)
		case _16_ARGB4444: return D3DFMT_A4R4G4B4; // 16-bit ARGB pixel format (4 bits per color)
		case _8_RGB332: return D3DFMT_R3G3B2; // 8-bit  RGB pixel format
		case _8_ARGB2222: return D3DFMT_UNKNOWN; // 8-bit  ARGB pixel format
		case _32_ABGR8888: return D3DFMT_A8B8G8R8; // 32-bit ABGR pixel format
		case _32_RGBA8888: return D3DFMT_UNKNOWN; // 32-bit RGBA pixel format
		case _32_BGRA8888: return D3DFMT_UNKNOWN; // 32-bit BGRA pixel format
		case _32_BGR888: return D3DFMT_X8B8G8R8; // 32-bit BGR pixel format
		case _24_BGR888: return D3DFMT_UNKNOWN; // 24-bit BGR pixel format
		case _16_BGR565: return D3DFMT_UNKNOWN; // 16-bit BGR pixel format
		case _16_BGR555: return D3DFMT_UNKNOWN; // 16-bit BGR pixel format (5 bits per color)
		case _16_ABGR1555: return D3DFMT_UNKNOWN; // 16-bit ABGR pixel format (5 bits per color + 1 bit for alpha)
		case _16_ABGR4444: return D3DFMT_UNKNOWN; // 16-bit ABGR pixel format (4 bits per color)
		case _DXT1: return D3DFMT_DXT1; // S3/DirectX Texture Compression 1
		case _DXT2: return D3DFMT_DXT2; // S3/DirectX Texture Compression 2
		case _DXT3: return D3DFMT_DXT3; // S3/DirectX Texture Compression 3
		case _DXT4: return D3DFMT_DXT4; // S3/DirectX Texture Compression 4
		case _DXT5: return D3DFMT_DXT5; // S3/DirectX Texture Compression 5
		case _16_V8U8: return D3DFMT_V8U8; // 16-bit Bump Map format (8 bits per color)
		case _32_V16U16: return D3DFMT_V16U16; // 32-bit Bump Map format (16 bits per color)
		case _16_L6V5U5: return D3DFMT_L6V5U5; // 16-bit Bump Map format with luminance
		case _32_X8L8V8U8: return D3DFMT_X8L8V8U8; // 32-bit Bump Map format with luminance
		case _8_ABGR8888_CLUT: return D3DFMT_UNKNOWN; // 8 bits indexed CLUT (ABGR)
		case _8_ARGB8888_CLUT: return D3DFMT_UNKNOWN; // 8 bits indexed CLUT (ARGB)
		case _4_ABGR8888_CLUT: return D3DFMT_UNKNOWN; // 4 bits indexed CLUT (ABGR)
		case _4_ARGB8888_CLUT: return D3DFMT_UNKNOWN; // 4 bits indexed CLUT (ARGB)
		default:
			return D3DFMT_UNKNOWN;
	}
}

VX_PIXELFORMAT D3DFormatToVxPixelFormat(D3DFORMAT ddpf)
{
	switch (ddpf)
	{
		case D3DFMT_A8R8G8B8: return _32_ARGB8888; // 32-bit ARGB pixel format with alpha
		case D3DFMT_X8R8G8B8: return _32_RGB888; // 32-bit RGB pixel format without alpha
		case D3DFMT_R8G8B8: return _24_RGB888; // 24-bit RGB pixel format
		case D3DFMT_R5G6B5: return _16_RGB565; // 16-bit RGB pixel format
		case D3DFMT_X1R5G5B5: return _16_RGB555; // 16-bit RGB pixel format (5 bits per color)
		case D3DFMT_A1R5G5B5: return _16_ARGB1555; // 16-bit ARGB pixel format (5 bits per color + 1 bit for alpha)
		case D3DFMT_A4R4G4B4: return _16_ARGB4444; // 16-bit ARGB pixel format (4 bits per color)
		case D3DFMT_R3G3B2: return _8_RGB332; // 8-bit  RGB pixel format
		case D3DFMT_UNKNOWN: return _8_ARGB2222; // 8-bit  ARGB pixel format
		case D3DFMT_A8B8G8R8: return _32_ABGR8888; // 32-bit ABGR pixel format
		case D3DFMT_X8B8G8R8: return _32_BGR888; // 32-bit BGR pixel format
		case D3DFMT_DXT1: return _DXT1; // S3/DirectX Texture Compression 1
		case D3DFMT_DXT2: return _DXT2; // S3/DirectX Texture Compression 2
		case D3DFMT_DXT3: return _DXT3; // S3/DirectX Texture Compression 3
		case D3DFMT_DXT4: return _DXT4; // S3/DirectX Texture Compression 4
		case D3DFMT_DXT5: return _DXT5; // S3/DirectX Texture Compression 5
		case D3DFMT_V8U8: return _16_V8U8; // 16-bit Bump Map format (8 bits per color)
		case D3DFMT_V16U16: return _32_V16U16; // 32-bit Bump Map format (16 bits per color)
		case D3DFMT_L6V5U5: return _16_L6V5U5; // 16-bit Bump Map format with luminance
		case D3DFMT_X8L8V8U8: return _32_X8L8V8U8; // 32-bit Bump Map format with luminance
		default: return UNKNOWN_PF;
	}
}

D3DFORMAT TextureDescToD3DFormat(CKTextureDesc *desc)
{
    return VxPixelFormatToD3DFormat(VxImageDesc2PixelFormat(desc->Format));
}

void D3DFormatToTextureDesc(D3DFORMAT ddpf, CKTextureDesc *desc)
{
    desc->Flags = CKRST_TEXTURE_VALID | CKRST_TEXTURE_RGB | CKRST_TEXTURE_ALPHA;
    VX_PIXELFORMAT vxpf = D3DFormatToVxPixelFormat(ddpf);
    VxPixelFormat2ImageDesc(vxpf, desc->Format);
}