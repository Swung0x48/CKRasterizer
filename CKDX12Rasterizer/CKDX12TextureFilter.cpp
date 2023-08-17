#include "CKDX12TextureFilter.h"

bool CKDX12TextureFilter::SetFilterMode(CKRST_TEXTURESTAGESTATETYPE Tss, VXTEXTURE_FILTERMODE Value)
{
    switch (Tss)
    {
        case CKRST_TSS_MINFILTER:
            min_filter_ = Value;
            modified_ = true;
            return true;
        case CKRST_TSS_MAGFILTER:
            mag_filter_ = Value;
            modified_ = true;
            return true;
    }
    return false;
}

D3D12_FILTER_TYPE CKDX12TextureFilter::Vx2D3DFilterMode(VXTEXTURE_FILTERMODE mode, bool *anisotropic)
{
    if (anisotropic)
        *anisotropic = false;
    switch (mode)
    {
        case VXTEXTUREFILTER_NEAREST:
        case VXTEXTUREFILTER_MIPNEAREST:
            return D3D12_FILTER_TYPE_POINT;
        case VXTEXTUREFILTER_ANISOTROPIC:
            if (anisotropic)
                *anisotropic = true;
        case VXTEXTUREFILTER_LINEAR:
        case VXTEXTUREFILTER_MIPLINEAR:
        case VXTEXTUREFILTER_LINEARMIPNEAREST:
        case VXTEXTUREFILTER_LINEARMIPLINEAR:
        default:
            return D3D12_FILTER_TYPE_LINEAR;
    }
}

D3D12_FILTER CKDX12TextureFilter::GetFilterMode(bool update)
{
    if (update)
        modified_ = false;
    D3D12_FILTER_TYPE magType = Vx2D3DFilterMode(min_filter_, &anisotropic_);
    D3D12_FILTER_TYPE minType = Vx2D3DFilterMode(mag_filter_, &anisotropic_);
    D3D12_FILTER_TYPE mipType = D3D12_FILTER_TYPE_POINT;
    if (anisotropic_)
        return D3D12_ENCODE_ANISOTROPIC_FILTER(D3D12_FILTER_REDUCTION_TYPE_STANDARD);

    return D3D12_ENCODE_BASIC_FILTER(minType, magType, mipType, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
}