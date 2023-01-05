#include "CKGLRasterizer.h"

CKGLRasterizerDriver::CKGLRasterizerDriver(CKGLRasterizer *rst)
{
	m_Owner = rst;
}

CKGLRasterizerDriver::~CKGLRasterizerDriver()
{
}

CKRasterizerContext *CKGLRasterizerDriver::CreateContext() {
    CKGLRasterizerContext* context = new CKGLRasterizerContext;
    context->m_Driver = this;
    context->m_Owner = static_cast<CKGLRasterizer *>(m_Owner);
    m_Contexts.PushBack(context);
    return context;
}

CKBOOL CKGLRasterizerDriver::InitializeCaps()
{
    m_Desc = "OpenGL Rasterizer";
    // pretend we have a 640x480 @ 16bpp mode for compatibility reasons
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER;
    m_DisplayModes.PushBack({640, 480, 16, 60});
    m_DisplayModes.PushBack({1024, 768, 32, 60});
    m_CapsUpToDate = TRUE;
    return 1;
}
