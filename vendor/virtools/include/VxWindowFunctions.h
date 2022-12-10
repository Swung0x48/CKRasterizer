#ifndef VXWINDOWFUNCTION_H
#define VXWINDOWFUNCTION_H

#include "VxMathDefines.h"
#include "XString.h"

struct VxImageDescEx;

// KeyBoard Functions
VX_EXPORT char VxScanCodeToAscii(XULONG scancode, unsigned char keystate[256]);
VX_EXPORT int VxScanCodeToName(XULONG scancode, char *keyName);

// Cursor function
/**************************************************
{filename:VXCURSOR_POINTER}
Summary:Appearance of mouse cursor.

See Also:VxSetCursor
***************************************************/
typedef enum VXCURSOR_POINTER
{
    VXCURSOR_NORMALSELECT = 1,	// Display the standard arrow cursor
    VXCURSOR_BUSY         = 2,	// Display the busy (hourglass) cursor
    VXCURSOR_MOVE         = 3,	// Display the move cursor
    VXCURSOR_LINKSELECT   = 4	// Display the link select (hand) cursor
} VXCURSOR_POINTER;

VX_EXPORT int VxShowCursor(XBOOL show);
VX_EXPORT XBOOL VxSetCursor(VXCURSOR_POINTER cursorID);

VX_EXPORT XWORD VxGetFPUControlWord();
VX_EXPORT void VxSetFPUControlWord(XWORD Fpu);
// Disable exceptions,round to nearest,single precision
VX_EXPORT void VxSetBaseFPUControlWord();

//-------Library Function

VX_EXPORT void VxAddLibrarySearchPath(char *path);
VX_EXPORT XBOOL VxGetEnvironmentVariable(char *envName, XString &envValue);
VX_EXPORT XBOOL VxSetEnvironmentVariable(char *envName, char *envValue);

VX_EXPORT XULONG VxEscapeURL(char *InURL, XString &OutURL);
VX_EXPORT void VxUnEscapeUrl(XString &str);

//------ Window Functions
VX_EXPORT WIN_HANDLE VxWindowFromPoint(CKPOINT pt);
VX_EXPORT XBOOL VxGetClientRect(WIN_HANDLE Win, CKRECT *rect);
VX_EXPORT XBOOL VxGetWindowRect(WIN_HANDLE Win, CKRECT *rect);
VX_EXPORT XBOOL VxScreenToClient(WIN_HANDLE Win, CKPOINT *pt);
VX_EXPORT XBOOL VxClientToScreen(WIN_HANDLE Win, CKPOINT *pt);

VX_EXPORT WIN_HANDLE VxSetParent(WIN_HANDLE Child, WIN_HANDLE Parent);
VX_EXPORT WIN_HANDLE VxGetParent(WIN_HANDLE Win);
VX_EXPORT XBOOL VxMoveWindow(WIN_HANDLE Win, int x, int y, int Width, int Height, XBOOL Repaint);
VX_EXPORT XString VxGetTempPath();
VX_EXPORT XBOOL VxMakeDirectory(char *path);
VX_EXPORT XBOOL VxRemoveDirectory(char *path);
VX_EXPORT XBOOL VxDeleteDirectory(char *path);
VX_EXPORT XBOOL VxGetCurrentDirectory(char *path);
VX_EXPORT XBOOL VxSetCurrentDirectory(char *path);
VX_EXPORT XBOOL VxMakePath(char *fullpath, char *path, char *file);
VX_EXPORT XBOOL VxTestDiskSpace(const char *dir, XULONG size);

VX_EXPORT int VxMessageBox(WIN_HANDLE hWnd, char *lpText, char *lpCaption, XULONG uType);

//------ Process access {secret}
VX_EXPORT XULONG VxGetModuleFileName(INSTANCE_HANDLE Handle, char *string, XULONG StringSize);
VX_EXPORT INSTANCE_HANDLE VxGetModuleHandle(const char *filename);

//------ Recreates the whole  file path (not the file itself) {secret}
VX_EXPORT XBOOL VxCreateFileTree(char *file);

//------ URL Download {secret}
VX_EXPORT XULONG VxURLDownloadToCacheFile(char *File, char *CachedFile, int szCachedFile);

//------ Bitmap Functions
VX_EXPORT BITMAP_HANDLE VxCreateBitmap(const VxImageDescEx &desc);
VX_EXPORT XBYTE *VxConvertBitmap(BITMAP_HANDLE Bitmap, VxImageDescEx &desc);
VX_EXPORT XBOOL VxCopyBitmap(BITMAP_HANDLE Bitmap, const VxImageDescEx &desc);
VX_EXPORT void VxDeleteBitmap(BITMAP_HANDLE BitmapHandle);
VX_EXPORT BITMAP_HANDLE VxConvertBitmapTo24(BITMAP_HANDLE _Bitmap);

VX_EXPORT VX_OSINFO VxGetOs();

typedef struct VXFONTINFO
{
    XString FaceName;
    int Height;
    int Weight;
    XBOOL Italic;
    XBOOL Underline;
} VXFONTINFO;

typedef enum VXTEXT_ALIGNMENT
{
    VXTEXT_CENTER  = 0x00000001,	// Text is centered when written
    VXTEXT_LEFT    = 0x00000002,	// Text is aligned to the left of the rectangle
    VXTEXT_RIGHT   = 0x00000004,	// Text is aligned to the right of the rectangle
    VXTEXT_TOP     = 0x00000008,	// Text is aligned to the top of the rectangle
    VXTEXT_BOTTOM  = 0x00000010,	// Text is aligned to the bottom of the rectangle
    VXTEXT_VCENTER = 0x00000020,	// Text is centered vertically
    VXTEXT_HCENTER = 0x00000040,	// Text is centered horizontally
} VXTEXT_ALIGNMENT;

//------ Font  Functions
VX_EXPORT FONT_HANDLE VxCreateFont(char *FontName, int FontSize, int Weight, XBOOL italic, XBOOL underline);
VX_EXPORT XBOOL VxGetFontInfo(FONT_HANDLE Font, VXFONTINFO &desc);
VX_EXPORT XBOOL VxDrawBitmapText(BITMAP_HANDLE Bitmap, FONT_HANDLE Font, char *string, CKRECT *rect, XULONG Align, XULONG BkColor, XULONG FontColor);
VX_EXPORT void VxDeleteFont(FONT_HANDLE Font);

#endif // VXWINDOWFUNCTION_H