#include "Common.hlsl"

static const dword AFLG_ALPHATESTEN = 0x10u;
static const dword AFLG_ALPHAFUNCMASK = 0xFu;
static const dword LFLG_LIGHTPOINT = 1UL;
static const dword LFLG_LIGHTSPOT = 2UL;
static const dword LFLG_LIGHTDIREC = 3UL;
static const dword LFLG_LIGHTEN = 1U << 31;
static const int MAX_ACTIVE_LIGHTS = 16;

struct light_t
{
    dword type; // highest bit as LIGHTEN
    float a0;
    float a1;
    float a2; // align
    float4 ambient; // a
    float4 diffuse; // a
    float4 specular; // a
    float4 direction; // a
    float4 position; // a
    float range;
    float falloff;
    float theta;
    float phi; // a
};

cbuffer PSCBuf : register(b0)
{
    dword alpha_flags;
    float alpha_thresh;
    dword _padding1;
    dword _padding2;
    light_t lights[MAX_ACTIVE_LIGHTS];
};

bool alpha_test(float in_alpha)
{
    switch (alpha_flags & AFLG_ALPHAFUNCMASK)
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
float4 main(float4 position : SV_POSITION, float4 normal : NORMAL, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 samp_color = texture2d.Sample(sampler_st, float2(texcoord.x, texcoord.y));
    if ((alpha_flags & AFLG_ALPHATESTEN) && !alpha_test(samp_color.a))
    {
        discard;
    }
    return samp_color;
}