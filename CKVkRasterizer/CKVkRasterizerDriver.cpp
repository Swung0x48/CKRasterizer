#include "CKVkRasterizer.h"

CKVkRasterizerDriver::CKVkRasterizerDriver(CKVkRasterizer *rst)
{
    m_Owner = rst;
}

CKVkRasterizerDriver::~CKVkRasterizerDriver()
{
}

CKRasterizerContext *CKVkRasterizerDriver::CreateContext() {
    //rst_ckctx->GetRenderManager()->SetRenderOptions("UseIndexBuffers", 1);
    CKVkRasterizerContext* context = new CKVkRasterizerContext;
    context->m_Driver = this;
    context->m_Owner = static_cast<CKVkRasterizer *>(m_Owner);
    context->vkinst = vkinst;
    context->vkphydev = vkphydev;
    context->vkdev = vkdev;
    context->gqidx = gqidx;
    context->pqidx = pqidx;
    m_Contexts.PushBack(context);
    return context;
}

bool operator==(const VxDisplayMode& a, const VxDisplayMode& b)
{
    return a.Bpp == b.Bpp &&
        a.Width == b.Width &&
        a.Height == b.Height &&
        a.RefreshRate == b.RefreshRate;
}

CKBOOL CKVkRasterizerDriver::InitializeCaps()
{
    DEVMODEA dm;
    
    // pretend we have a 640x480 @ 16bpp mode for compatibility reasons
    VxDisplayMode mode {640, 480, 16, 60};
    m_DisplayModes.PushBack(mode);
    
    ZeroMemory(&dm, sizeof(dm));
    for (DWORD modeIndex = 0; ; ++modeIndex)
    {
        if (!EnumDisplaySettingsA(m_Adapter.DeviceName, modeIndex, &dm))
            break;
        
        mode.Width = dm.dmPelsWidth;
        mode.Height = dm.dmPelsHeight;
        mode.Bpp = dm.dmBitsPerPel;
        mode.RefreshRate = dm.dmDisplayFrequency;
        if (m_DisplayModes.Back() != mode)
            m_DisplayModes.PushBack(mode);
    }

    VkPhysicalDeviceProperties phydevprop;
    vkGetPhysicalDeviceProperties(vkphydev, &phydevprop);
    m_Desc << " " << phydevprop.deviceName;
    uint32_t ecnt = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ecnt, nullptr);

    ZeroMemory(&m_3DCaps, sizeof(m_3DCaps));
    ZeroMemory(&m_2DCaps, sizeof(m_2DCaps));
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_CANDOINDEXBUFFER;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_GLATTENUATIONMODEL;
    m_3DCaps.CKRasterizerSpecificCaps |= CKRST_SPECIFICCAPS_HARDWARETL;
    m_3DCaps.MaxNumberTextureStage = 1; //?
    m_3DCaps.MaxNumberBlendStage = 1;   //fake it until we make it
    m_3DCaps.MaxActiveLights = MAX_ACTIVE_LIGHTS;
    m_3DCaps.MinTextureWidth = 1;       //we are using texture of width 1 for blank textures
    m_3DCaps.MinTextureHeight = 1;      //so we know it must work... or do we?
    m_3DCaps.MaxTextureWidth = 1024;    //we know OpenGL guarantees this to be at least 1024...
    m_3DCaps.MaxTextureHeight = 1024;   //and I'm too lazy to create a context here...
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_DIRECTIONALLIGHTS;
    m_3DCaps.VertexCaps |= CKRST_VTXCAPS_TEXGEN;
    m_3DCaps.AlphaCmpCaps = 0xff;       //we have TECHNOLOGY
    m_3DCaps.ZCmpCaps = 0xff;           //who wouldn't be 0xff here?
    m_3DCaps.TextureAddressCaps = 0x1f; //everything
    m_3DCaps.TextureCaps |= CKRST_TEXTURECAPS_PERSPECTIVE; //not only do we support it, it's ALWAYS on
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_MASKZ;      //glDepthMask
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CONFORMANT; //because non-conformant OpenGL drivers don't exist /s
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLNONE;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCW;
    m_3DCaps.MiscCaps |= CKRST_MISCCAPS_CULLCCW;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_ZTEST;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_FOGPIXEL; //vertex fog? what's that paleocene technology?
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WBUFFER;
    m_3DCaps.RasterCaps |= CKRST_RASTERCAPS_WFOG;     //w fog hardcoded in shader
    m_3DCaps.SrcBlendCaps = 0x1fff;     //everything
    m_3DCaps.DestBlendCaps = 0x1fff;    //ditto
    m_2DCaps.Family = CKRST_OPENGL;
    m_2DCaps.Caps = (CKRST_2DCAPS_3D | CKRST_2DCAPS_GDI);
    m_CapsUpToDate = TRUE;
    return 1;
}
