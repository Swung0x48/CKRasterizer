#include "CKGlSystemInfo.h"
#include <XArray.h>

XBOOL CKGlSystemInfo::m_Inited = FALSE;
WNDCLASSEXA CKGlSystemInfo::m_WndClass;
XArray<VxDisplayMode> displayModes;
HWND CKGlSystemInfo::m_AppWindow;
XBOOL CKGlSystemInfo::m_FoundHardware;
XBOOL CKGlSystemInfo::m_FoundGeneric;
XBOOL CKGlSystemInfo::m_FoundStereo;
XSArray<DWORD> CKGlSystemInfo::m_PixelFormat;

LRESULT WINAPI GL_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (Msg > 0x1c) {
        if (Msg == 513) {
            LONG userdata = GetWindowLongPtrA(hWnd, GWLP_USERDATA);
            ::SendMessageA(*(HWND*)(*(DWORD*)(userdata + 4) + 52), 0x201u, wParam, lParam);
        }
    }
    else {
        switch (Msg) {
        case WM_ACTIVATEAPP:
            LONG userdata = GetWindowLongPtrA(hWnd, GWLP_USERDATA); // some userdata struct?
            if (userdata) {
                if (wParam) {
                    int* ptr = *(int**)(userdata + 4);
                    if (ptr[11]) {
                        SetWindowPos(*(HWND*)(userdata + 20), (HWND)(-3 | 0x2), 0, 0, ptr[5], ptr[6], 0);
                        SetFocus(*(HWND*)(userdata + 20));
                        wglMakeCurrent(*(HDC*)(userdata + 16), *(HGLRC*)(userdata + 12));
                    }
                }
            }
        }
    }
    return ::DefWindowProc(hWnd, Msg, wParam, lParam);
}

XBOOL CKGlSystemInfo::Init(HWND hWnd)
{
    if (m_Inited)
        return TRUE;

    m_AppWindow = hWnd;

    m_WndClass.cbSize = sizeof(WNDCLASSEXA);
    m_WndClass.style = 32;
    m_WndClass.lpfnWndProc = GL_WndProc;
    m_WndClass.cbClsExtra = 0;
    m_WndClass.cbWndExtra = 4;
    m_WndClass.hbrBackground = (HBRUSH)::GetStockObject(5);
    m_WndClass.lpszClassName = "CKRasterizerGlWndClassaa";
    ::RegisterClassExA(&m_WndClass); // replace with glfw impl
    DEVMODEA* devmode = new DEVMODEA;
    devmode->dmSize = 156;
    BOOL enumSucceed = FALSE;
    do {
        enumSucceed = EnumDisplaySettingsA(0, 0, devmode);
        VxDisplayMode mode;
        mode.Height = devmode->dmPelsHeight;
        mode.RefreshRate = devmode->dmDisplayFrequency;
        mode.Bpp = devmode->dmBitsPerPel;
        mode.Width = devmode->dmPelsWidth;
        displayModes.PushBack(mode);
    } while (enumSucceed);
    delete devmode;
    HDC dc = GetDC(m_AppWindow);
    PIXELFORMATDESCRIPTOR ppfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
        PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
        32,                   // Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,                   // Number of bits for the depthbuffer
        8,                    // Number of bits for the stencilbuffer
        0,                    // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    ::ChoosePixelFormat(dc, &ppfd);
    ::DescribePixelFormat(dc, 1, sizeof(PIXELFORMATDESCRIPTOR), &ppfd);
    int maxPixelFormatIndex = DescribePixelFormat(dc, 0, 0, 0);
    if (maxPixelFormatIndex <= 0)
        return FALSE;

    PIXELFORMATDESCRIPTOR pfd;
    for (int i = 1; i <= maxPixelFormatIndex; ++i) {
        if (::DescribePixelFormat(dc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
            if (!pfd.iPixelType && (pfd.dwFlags & PFD_SUPPORT_OPENGL) != 0) { // surface supports OpenGL
                XBOOL v20 = ((pfd.dwFlags & PFD_GENERIC_FORMAT) != 0);
                XBOOL v21 = ((pfd.dwFlags & PFD_GENERIC_ACCELERATED) != 0);
                XBOOL v27 = pfd.dwFlags & PFD_DOUBLEBUFFER;
                if (v27 != 0 && (pfd.dwFlags & PFD_DRAW_TO_WINDOW) != 0 ||
                    (pfd.dwFlags & PFD_SUPPORT_GDI) != 0 && (pfd.dwFlags & PFD_DRAW_TO_BITMAP) != 0) {
                    DWORD pixelFormat = 1;
                    if (v20 == v21) {
                        m_FoundHardware = TRUE;
                        pixelFormat = 9;
                        if ((pfd.dwFlags & PFD_STEREO) != 0) {
                            m_FoundStereo = TRUE;
                            pixelFormat = 25;
                        }
                    }
                    else {
                        m_FoundGeneric = 1;
                    }
                    if (v27) {
                        if ((pfd.dwFlags & PFD_DRAW_TO_WINDOW) != 0) {
                            pixelFormat |= 6u;
                        }
                    }
                    m_PixelFormat.PushBack(pixelFormat);
                }
            }
        }
    }
    ::ReleaseDC(m_AppWindow, dc);
    m_Inited = TRUE;
    return TRUE;
}

XBOOL CKGlSystemInfo::Close()
{
    if (m_Inited) {
        m_Inited = FALSE;
        HMODULE m = GetModuleHandleA("CKGLRasterizer.dll");
        UnregisterClassA("CKRasterizerGlWndClassaa", m);
    }
        
    return TRUE;
}

int CKGlSystemInfo::ChoosePixelFormat()
{
    return 0;
}
