#ifndef VXIMAGEDESCEX_H
#define VXIMAGEDESCEX_H

#include "VxMathDefines.h"

/****************************************************************
Name: VxImageDescEx

Summary: Enhanced Image description

Remarks:
The VxImageDescEx holds basically an VxImageDesc with additional support for
Colormap, Image pointer and is ready for future enhancements.


****************************************************************/
typedef struct VxImageDescEx
{
    int Size;    // Size of the structure
    XULONG Flags; // Reserved for special formats (such as compressed ) 0 otherwise

    int Width;  // Width in pixel of the image
    int Height; // Height in pixel of the image
    union
    {
        int BytesPerLine;   // Pitch (width in bytes) of the image
        int TotalImageSize; // For compressed image (DXT1...) the total size of the image
    };
    int BitsPerPixel; // Number of bits per pixel
    union
    {
        XULONG RedMask;    // Mask for Red component
        XULONG BumpDuMask; // Mask for Bump Du component
    };
    union
    {
        XULONG GreenMask;  // Mask for Green component
        XULONG BumpDvMask; // Mask for Bump Dv component
    };
    union
    {
        XULONG BlueMask;    // Mask for Blue component
        XULONG BumpLumMask; // Mask for Luminance component
    };
    XULONG AlphaMask; // Mask for Alpha component

    short BytesPerColorEntry; // ColorMap Stride
    short ColorMapEntries;    // If other than 0 image is palletized

    XBYTE *ColorMap; // Palette colors
    XBYTE *Image;    // Image

public:
    VxImageDescEx()
    {
        Size = sizeof(VxImageDescEx);
        memset((XBYTE *)this + 4, 0, Size - 4);
    }

    void Set(const VxImageDescEx &desc)
    {
        Size = sizeof(VxImageDescEx);
        if (desc.Size < Size)
            memset((XBYTE *)this + 4, 0, Size - 4);
        if (desc.Size > Size)
            return;
        memcpy((XBYTE *)this + 4, (XBYTE *)&desc + 4, desc.Size - 4);
    }

    XBOOL HasAlpha()
    {
        return ((AlphaMask != 0) || (Flags >= _DXT1));
    }

    int operator==(const VxImageDescEx &desc)
    {
        return (Size == desc.Size &&
                Height == desc.Height && Width == desc.Width &&
                BitsPerPixel == desc.BitsPerPixel && BytesPerLine == desc.BytesPerLine &&
                RedMask == desc.RedMask && GreenMask == desc.GreenMask &&
                BlueMask == desc.BlueMask && AlphaMask == desc.AlphaMask &&
                BytesPerColorEntry == desc.BytesPerColorEntry && ColorMapEntries == desc.ColorMapEntries);
    }

    int operator!=(const VxImageDescEx &desc)
    {
        return (Size != desc.Size ||
                Height != desc.Height || Width != desc.Width ||
                BitsPerPixel != desc.BitsPerPixel || BytesPerLine != desc.BytesPerLine ||
                RedMask != desc.RedMask || GreenMask != desc.GreenMask ||
                BlueMask != desc.BlueMask || AlphaMask != desc.AlphaMask ||
                BytesPerColorEntry != desc.BytesPerColorEntry || ColorMapEntries != desc.ColorMapEntries);
    }

} VxImageDescEx;

#endif // VXIMAGEDESCEX_H