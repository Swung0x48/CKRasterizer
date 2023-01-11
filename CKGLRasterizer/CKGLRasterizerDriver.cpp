#include "CKGLRasterizer.h"
#include <CKContext.h>
#include <CKRenderManager.h>

CKGLRasterizerDriver::CKGLRasterizerDriver(CKGLRasterizer *rst)
{
	m_Owner = rst;
}

CKGLRasterizerDriver::~CKGLRasterizerDriver()
{
}

CKRasterizerContext *CKGLRasterizerDriver::CreateContext() {
    rst_ckctx->GetRenderManager()->SetRenderOptions("UseIndexBuffers", 1);
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
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_GLATTENUATIONMODEL;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    m_3DCaps.MaxActiveLights = 16;
    m_2DCaps.Family = CKRST_OPENGL;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
    m_DisplayModes.PushBack({640, 480, 16, 60});
    m_DisplayModes.PushBack({1024, 768, 32, 60});
    m_DisplayModes.PushBack({1600, 1200, 32, 60});
    m_DisplayModes.PushBack({1920, 1440, 32, 60});
    m_CapsUpToDate = TRUE;
    return 1;
}
