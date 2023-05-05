#include <cstdio>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "CKDX11Rasterizer.h"

extern DWORD main_thread_id;

HHOOK hook = NULL;
HHOOK window_hook = NULL;
CKDX11RasterizerContext *r;

// LRESULT CALLBACK keyboard_handler(int code, WPARAM kc, LPARAM rc)
// {
//     if ((rc & 0x20000000) && !(rc & 0x40000000)) // ALT, key press
//     {
//         switch (kc)
//         {
//             case 'D':
//                 r->toggle_fullscreen();
//                 break;
//         }
//     }
//
//     return CallNextHookEx(NULL, code, kc, rc);
// }

LRESULT CALLBACK window_handler(int code, WPARAM wParam, LPARAM lParam)
{
    CWPRETSTRUCT* msg = (CWPRETSTRUCT *) lParam;
    if (msg->message == WM_SIZE)
    {
        if (r)
            // r->m_NeedBufferResize = TRUE;
            r->resize_buffers();
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

void debug_destroy()
{
#if defined(DEBUG) || defined(_DEBUG)
    fprintf(stderr, "debug_destroy\n");
#endif
    r = nullptr;
    // UnhookWindowsHookEx(hook);
    UnhookWindowsHookEx(window_hook);
    window_hook = NULL;
}

void debug_setup(CKDX11RasterizerContext *rst)
{
    if (r == rst)
        return;
    if (r && r != rst)
        debug_destroy();
    
#if defined(DEBUG) || defined(_DEBUG)
    fprintf(stderr, "debug_setup\n");
#endif
    r = rst;
    // hook = SetWindowsHookExA(WH_KEYBOARD, &keyboard_handler, NULL, main_thread_id);
    window_hook = SetWindowsHookExA(WH_CALLWNDPROCRET, &window_handler, NULL, main_thread_id);
}
