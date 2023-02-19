#include "Common.hlsl"

static const dword AFLG_ALPHATESTEN = 0x10u;
static const dword AFLG_ALPHAFUNCMASK = 0xFu;

static const dword LSW_SPECULAREN = 1U << 0;
static const dword LSW_LIGHTINGEN = 1U << 0;
static const dword LSW_VRTCOLOREN = 1U << 0;

static const dword LFLG_LIGHTPOINT = 1UL;
static const dword LFLG_LIGHTSPOT = 2UL;
static const dword LFLG_LIGHTDIREC = 3UL;  // unused
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

struct material_t
{
    float4 diffuse;
    float4 ambient;
    float4 specular;
    float4 emissive;
    float specular_power;
};

cbuffer PSCBuf : register(b0)
{
    material_t material;
    dword alpha_flags;
    float alpha_thresh;
    dword global_light_switches;
    float3 view_position;
    dword _padding;
    light_t lights[MAX_ACTIVE_LIGHTS];
};

static const float4 zero4f = float4(0., 0., 0., 0.);

float3x4 light_point(light_t l, float3 normal, float3 fpos, float3 vdir, bool spec_enabled)
{
    float3 ldir = normalize(l.position.xyz - fpos);
    float dist = length(l.position.xyz - fpos);
    if (dist > l.range)
    {
        return float3x4(zero4f, zero4f, zero4f);
    }
    // CK2_3D always gives us DX5 light parameters... thus the DX5 light formula here.
    dist = 1. - dist / l.range;
    float atnf = l.a0 + (l.a1 * dist + l.a2 * dist * dist);
    float diff = max(dot(normal, ldir), 0.);
    float4 ambient = l.ambient * material.ambient;
    float4 diffuse = diff * l.diffuse * material.diffuse;
    float4 specular = float4(0., 0., 0., 0.);
    if (spec_enabled
#ifdef DEBUG
        && (global_light_switches & LSW_SPCL_OVERR_FORCE) == 0U
#endif
    )
    {
        float3 refldir = reflect(-ldir, normal);
        float specl = pow(clamp(dot(vdir, refldir), 0., 1.), material.specular_power);
        specular = l.specular * material.specular * specl;
#ifdef DEBUG
        if ((global_light_switches & LSW_SPCL_OVERR_ONLY) != 0U)
            return float3x4(zero4f, zero4f, spc * atnf);
#endif
    }
    return float3x4(ambient * atnf, diffuse * atnf, specular * atnf);
}

float3x4 light_directional(light_t l, float3 normal, float3 vdir, bool spec_enabled)
{
    float3 ldir = normalize(-l.direction.xyz);
    float diff = max(dot(normal, ldir), 0.);
    float4 amb = l.ambient * material.ambient;
    float4 dif = diff * l.diffuse * material.diffuse;
    float4 spc = zero4f;
    if (spec_enabled
#ifdef DEBUG
        && (global_light_switches & LSW_SPCL_OVERR_FORCE) == 0U
#endif
    )
    {
        float3 refldir = reflect(-ldir, normal);
        float specl = pow(clamp(dot(vdir, refldir), 0., 1.), material.specular_power);
        // direct3d9 specular strength formula
        // float3 hv = normalize(normalize(vpos - fpos) + ldir);
        // specl = pow(dot(normal, hv), material.spcl_strength);
        spc = l.specular * material.specular * specl;
#ifdef DEBUG
        if ((global_light_switches & LSW_SPCL_OVERR_ONLY) != 0U)
            return float3x4(zero4f, zero4f, spc);
#endif
    }
    return float3x4(amb, dif, spc);
}

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

float3x4 component_add(float3x4 a, float3x4 b)
{
    float3x4 r;
    r[0] = a[0] + b[0];
    r[1] = a[1] + b[1];
    r[2] = a[2] + b[2];
    return r;
}

float4 clamp_color(float4 c) { return clamp(c, float4(0, 0, 0, 0), float4(1, 1, 1, 1)); }
float4 accum_light(float3x4 c) { return c[0] + c[1] + c[2]; }
float4 accum_light_e(float4x4 c) { return c[0] + c[1] + c[2] + c[3]; }

Texture2D texture2d;
SamplerState sampler_st;
float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 norm = normalize(input.normal);
    float3 vdir = normalize(view_position - input.position);
    float4 color = float4(1., 1., 1., 1.);
    float3x4 lighting_colors = float3x4(zero4f, zero4f, zero4f);
    float4x4 lighting_colors_e = float4x4(zero4f, zero4f, zero4f, zero4f);
    if ((global_light_switches & LSW_VRTCOLOREN) != 0U)
        color = input.color;
    if ((global_light_switches & LSW_LIGHTINGEN) != 0U)
    {
        for (uint i = 0U; i < 16U; ++i)
        {
            switch (lights[i].type)
            {
                case 1U:
                    lighting_colors = component_add(
                        lighting_colors,
                        light_point(lights[i], norm, input.position, vdir, (global_light_switches & LSW_SPECULAREN) != 0U));
                    break;
                case 3U:
                    lighting_colors = component_add(
                        lighting_colors,
                        light_directional(lights[i], norm, vdir, (global_light_switches & LSW_SPECULAREN) != 0U));
                    break;
            }
        }
        lighting_colors[0] = clamp_color(lighting_colors[0]);
        lighting_colors[1] = clamp_color(lighting_colors[1]);
        lighting_colors[2] = clamp_color(lighting_colors[2]);
        color = accum_light(lighting_colors);
        lighting_colors_e = float4x4(lighting_colors[0], lighting_colors[1], lighting_colors[2], zero4f);
#ifdef DEBUG
        if ((global_light_switches & LSW_SPECULAREN) == 0U ||
            (global_light_switches & LSW_SPECULAREN) != 0U && (global_light_switches & LSW_SPCL_OVERR_ONLY) == 0U)
#endif
        {
            lighting_colors_e[3] = material.emissive;
            color = accum_light_e(lighting_colors_e);
            if (material.diffuse.a < 1)
                color.a = material.diffuse.a;
        }
    }

    float4 samp_color = texture2d.Sample(sampler_st, float2(input.texcoord.x, input.texcoord.y));
    color = 
        (max(lighting_colors_e[0] + lighting_colors_e[1] + lighting_colors_e[3], float4(1., 1., 1., 1.))
            + lighting_colors_e[2]) * samp_color;

    if ((alpha_flags & AFLG_ALPHATESTEN) && !alpha_test(color.a))
    {
        discard;
    }
    return color;
}