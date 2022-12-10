#pragma once
#include <CKRasterizer.h>
#include <Windows.h>
class CKGlSystemInfo
{
public:
	static XBOOL Init(HWND hwnd);
	static XBOOL Close();
	static int ChoosePixelFormat();
//private:
	static XBOOL m_Inited;
	static HWND m_AppWindow;
	static WNDCLASSEXA m_WndClass;
	static XBOOL m_FoundHardware;
	static XBOOL m_FoundGeneric;
	static XBOOL m_FoundStereo;
	static XSArray<DWORD> m_PixelFormat;
};

