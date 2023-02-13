#include "Common.hlsl"

bool alpha_test(float in_alpha)
{
    switch (alpha_flags & 0xFU)
    {
        case 1U: return false;
        case 2U: return in_alpha <  alpha_thresh;
        case 3U: return in_alpha == alpha_thresh;
        case 4U: return in_alpha <= alpha_thresh;
        case 5U: return in_alpha >  alpha_thresh;
        case 6U: return in_alpha != alpha_thresh;
        case 7U: return in_alpha >= alpha_thresh;
        case 8U:
        default: return true;
    }
}

Texture2D texture2d;
SamplerState sampler_st;
float4 PShader(float4 position : SV_POSITION, float4 color : COLOR, float2 texcoord : TEXCOORD) : SV_TARGET
{
    if ((alpha_flags & AFLG_ALPHATESTEN) && !alpha_test(color.a)) {
        discard;
    }
    return texture2d.Sample(sampler_st, float2(texcoord.x, 1 - texcoord.y));
}