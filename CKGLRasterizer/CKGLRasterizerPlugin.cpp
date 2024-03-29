#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <CKContext.h>

static CKPluginInfo plugininfo;
CKContext *rst_ckctx = nullptr;
DWORD main_thread_id = 0;

#define CKGLRST CKGUID(0x10241024,0x10241024)

CKERROR get_ck_context(CKContext* context)
{
	main_thread_id = GetCurrentThreadId();
	rst_ckctx = context;
	return CK_OK;
}

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 1; }

PLUGIN_EXPORT CKPluginInfo* CKGetPluginInfo(int Index)
{
	plugininfo.m_Author             = "NOT Virtools"; 
	plugininfo.m_Description        = "OpenGL Rasterizer Auxiliary Plugin";
	plugininfo.m_Extension          = "";
	plugininfo.m_Type               = CKPLUGIN_EXTENSION_DLL;
	plugininfo.m_Version            = 0;
	plugininfo.m_InitInstanceFct    = &get_ck_context;
	plugininfo.m_ExitInstanceFct    = NULL;
	plugininfo.m_GUID               = CKGLRST;
	plugininfo.m_Summary            = "Force CK2_3D to use index buffers";
	return &plugininfo;
}