#pragma once

#include <d3d11.h>
#include <VxDefines.h>
class CKDX11TextureFilter
{
public:
    static D3D11_FILTER_TYPE Vx2D3DFilterMode(VXTEXTURE_FILTERMODE mode, bool *anisotropic);
    bool SetFilterMode(CKRST_TEXTURESTAGESTATETYPE Tss, VXTEXTURE_FILTERMODE Value);
    D3D11_FILTER GetFilterMode(bool update);
    
    VXTEXTURE_FILTERMODE min_filter_ = VXTEXTUREFILTER_NEAREST;
    VXTEXTURE_FILTERMODE mag_filter_ = VXTEXTUREFILTER_NEAREST;
    // VXTEXTURE_FILTERMODE mip_filter_;
    bool anisotropic_ = false;
    bool modified_ = false;
};
