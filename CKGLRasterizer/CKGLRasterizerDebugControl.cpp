#include <cstdio>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "CKGLRasterizer.h"

extern DWORD main_thread_id;

HHOOK hook = NULL;
CKGLRasterizerContext *r;

LRESULT CALLBACK keyboard_handler(int code, WPARAM kc, LPARAM rc)
{
	if (rc & 0x20000000) //ALT
    {
		switch (kc)
        {
            case 'Q': r->toggle_console(); break;
            case 'A': r->set_step_mode(1); break;
            case 'Z': r->set_step_mode(2); break;
		}
	}

	return CallNextHookEx(NULL, code, kc, rc);
}

void debug_setup(CKGLRasterizerContext *rst)
{
	r = rst;
	hook = SetWindowsHookExA(WH_KEYBOARD, &keyboard_handler, NULL, main_thread_id);
}
