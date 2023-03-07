#include "Common.hlsl"

static const dword AFLG_ALPHATESTEN = 0x10u;
static const dword AFLG_ALPHAFUNCMASK = 0xFu;

static const dword LSW_LIGHTINGEN = 1U << 0;
static const dword LSW_SPECULAREN = 1U << 1;
static const dword LSW_VRTCOLOREN = 1U << 2;

static const dword LFLG_LIGHTPOINT = 1U;
static const dword LFLG_LIGHTSPOT = 2U; // unused
static const dword LFLG_LIGHTDIREC = 3U;
static const dword LFLG_LIGHTTYPEMASK = 7U;
static const dword LFLG_LIGHTEN = 1U << 31;
static const int MAX_ACTIVE_LIGHTS = 16;
static const int MAX_TEX_STAGES = 2;

static const dword NULL_TEXTURE_MASK = (1 << (MAX_TEX_STAGES + 1)) - 1;

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

struct texcomb_t
{
    dword op; //@0, bit 0-3: color op, bit 4-7: alpha op, bit 31: dest
    dword cargs; //@4, bit 0-7: arg1, bit 8-15: arg2, bit 16-23: arg3
    dword aargs; //@8, ditto but for alpha
    dword constant; //@12
    // stride per element: 16
};

cbuffer PSCBuf : register(b0)
{
    material_t material;
    dword alpha_flags;
    float alpha_thresh;
    dword global_light_switches;
    float3 view_position;
    dword fvf;
    light_t lights[MAX_ACTIVE_LIGHTS];
    texcomb_t tex_combinator[MAX_TEX_STAGES];
};

static const float4 zero4f = float4(0., 0., 0., 0.);

float3x4 light_unified(light_t l, float3 normal, float4 frag_diffuse, float3 fpos, float3 vdir, bool spec_enabled)
{
    bool use_vert_color = (global_light_switches & LSW_VRTCOLOREN) != 0U;
    float atnf = 1.;
    float3 ldir = normalize(l.position.xyz - fpos);
    if ((l.type & 0xFU) == 1U || (l.type & 0xFU) == 2U)
    {
        float dist = length(l.position.xyz - fpos);
        if (dist > l.range)
            return float3x4(zero4f, zero4f, zero4f);
        dist = 1. - dist / l.range;
        atnf = l.a0 + (l.a1 * dist + l.a2 * dist * dist);
        if (l.type == 2U) // spotlight factor...
        {
            float rho = dot(normalize(-l.direction.xyz), ldir);
            if (rho <= cos(l.phi / 2))
                return float3x4(zero4f, zero4f, zero4f);
            if (rho <= cos(l.theta / 2))
            {
                float spf = pow((rho - cos(l.phi / 2)) / (cos(l.theta / 2) - cos(l.phi / 2)), l.falloff);
                atnf *= spf;
            }
        }
    }
    else
        ldir = normalize(-l.direction.xyz);
    // CK2_3D always gives us DX5 light parameters... thus the DX5 light formula here.
    float diff = max(dot(normal, ldir), 0.);
    float4 amb = l.ambient * material.ambient;
    float4 dif = diff * l.diffuse * material.diffuse;
    if (use_vert_color)
        dif = diff * l.diffuse * frag_diffuse;
    float4 spc = zero4f;
    if (spec_enabled
#ifdef DEBUG
        && (lighting_switches & LSW_SPCL_OVERR_FORCE) == 0U
#endif
    )
    {
        float3 refldir = reflect(-ldir, normal);
        float specl = pow(clamp(dot(vdir, refldir), 0., 1.), material.specular_power);
        // direct3d9 specular strength formula
        // float3 hv = normalize(vdir + ldir);
        // specl = pow(dot(normal, hv), material.spcl_strength);
        spc = l.specular * material.specular * specl;
        if (use_vert_color)
            spc = l.specular * frag_diffuse * specl;
#ifdef DEBUG
        if ((lighting_switches & LSW_SPCL_OVERR_ONLY) != 0U)
            return float3x4(zero4f, zero4f, spc * atnf);
#endif
    }
    return float3x4(amb * atnf, dif * atnf, spc * atnf);
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
float4 accum_light(float3x4 c) { return c[0]; }
float4 accum_light_e(float4x4 c) { return c[0] + c[1] + c[2] + c[3]; }

float4 combine_value(uint mode, float4 a, float4 b, float factor)
{
    switch (mode)
    {
        case 2U:
            return a;
        case 3U:
            return b;
        case 4U:
            return a * b;
        case 5U:
            return 2 * (a * b);
        case 6U:
            return 4 * (a * b);
        case 7U:
            return a + b;
        case 8U:
            return a + b - float4(.5, .5, .5, .5);
        case 9U:
            return 2 * (a + b - float4(.5, .5, .5, .5));
        case 10U:
            return a - b;
        case 11U:
            return a + b - a * b;
        case 13U:
            return lerp(b, a, factor);
        default:
            return zero4f;
    }
}

float4 dw2color(uint c)
{
    return float4(float((c >> 16U) & 0xFFU) / 255., float((c >> 8U) & 0xFFU) / 255., float((c >> 0U) & 0xFFU) / 255.,
                float((c >> 24U) & 0xFFU) / 255.);
}

float4 select_argument(uint stage, uint source, float4 tex, float4 accum, float4 tmp, float4x4 light_color)
{
    float4 ret = zero4f;
    switch (source) // D3DTA_*
    {
        case 0U:
            ret = light_color[1] + light_color[3];
            break; // DIFFUSE
        case 1U:
            if (stage == 0U) // CURRENT
                ret = light_color[1] + light_color[3]; // CURRENT at stage 0 behaves as DIFFUSE
            else
                ret = accum;
            break;
        case 2U:
            ret = tex;
            break; // TEXTURE
        case 3U:
            ret = zero4f;
            break; // TFACTOR, unsupported
        case 4U:
            ret = light_color[2];
            break; // SPECULAR
        case 5U:
            ret = tmp;
            break; // TEMP
        case 6U:
            ret = dw2color(tex_combinator[stage].constant);
            break; // CONSTANT
    }
    if ((source & 0x10U) != 0U)
        ret = float4(1., 1., 1., 1.) - ret;
    if ((source & 0x20U) != 0U)
        ret = float4(ret.a, ret.a, ret.a, ret.a);
    return ret;
}

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);
Texture2D texture1 : register(t1);
SamplerState sampler1 : register(s1);

float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 norm = normalize(input.normal);
    float3 vdir = normalize(view_position - input.worldpos);
    float4 color = float4(1., 1., 1., 1.);
    float3x4 lighting_colors = float3x4(zero4f, zero4f, zero4f);
    float4x4 lighting_colors_e = float4x4(zero4f, zero4f, zero4f, zero4f);
    if ((global_light_switches & LSW_VRTCOLOREN) != 0U)
        color = input.color;
    if ((global_light_switches & LSW_LIGHTINGEN) != 0U)
    {
        for (uint i = 0U; i < 16U; ++i)
        {
            if ((lights[i].type & 0x80000000U) != 0)
                lighting_colors = component_add(lighting_colors,
                                                light_unified(lights[i], norm, input.color, input.worldpos, vdir,
                                                              (global_light_switches & LSW_SPECULAREN) != 0U));
        }
        lighting_colors[0] = clamp_color(lighting_colors[0]);
        lighting_colors[1] = clamp_color(lighting_colors[1]);
        lighting_colors[2] = clamp_color(lighting_colors[2]);
        color = accum_light(lighting_colors);
        // lighting_colors_e[0] = lighting_colors[0];
        // lighting_colors_e[1] = lighting_colors[1];
        // lighting_colors_e[2] = lighting_colors[2];
        lighting_colors_e = float4x4(lighting_colors[0], lighting_colors[1], lighting_colors[2], zero4f);
#ifdef DEBUG
        if ((global_light_switches & LSW_SPECULAREN) == 0U ||
            (global_light_switches & LSW_SPECULAREN) != 0U && (global_light_switches & LSW_SPCL_OVERR_ONLY) == 0U)
#endif
        {
            lighting_colors_e[3] = material.emissive;
            color = accum_light_e(lighting_colors_e);
            color.a = clamp(color.a, 0, 1);
            if (material.diffuse.a < 1)
                color.a = material.diffuse.a;
        }
    }
    else
        color = input.color;

    int ntex = (fvf & VF_TEX2) ? 2 : 1;
    if (ntex > 1)
    {
        float4 accumulator = float4(.5, 0, 0, 1);
        float2x4 color_at_stage = float2x4(zero4f, zero4f);
        for (int i = 0; i < ntex; ++i)
        {
            float4 sampled_color = float4(1, 1, 1, 1);
            switch (i)
            {
                case 0U:
                    sampled_color = texture0.Sample(sampler0, input.texcoord0);
                    break;
                case 1U:
                    sampled_color = texture1.Sample(sampler1, input.texcoord1);
                    break;
                default:
                    break;
            }
            uint op = tex_combinator[i].op;
            uint cargs = tex_combinator[i].cargs;
            uint aargs = tex_combinator[i].aargs;

            uint cop = op & 0xfU;
            uint aop = (op >> 4) & 0xfu;
            uint dst = (op >> 31) & 0x3fu; // 1: temp, 0: accumulator
            uint ca1 = cargs & 0xffU;
            uint ca2 = (cargs >> 8) & 0xffU;
            uint aa1 = aargs & 0xffU;
            uint aa2 = (aargs >> 8) & 0xffU;
            float4 cv1 = select_argument(i, ca1, sampled_color, accumulator, color_at_stage[i], lighting_colors_e);
            float4 cv2 = select_argument(i, ca2, sampled_color, accumulator, color_at_stage[i], lighting_colors_e);
            float4 av1 = select_argument(i, aa1, sampled_color, accumulator, color_at_stage[i], lighting_colors_e);
            float4 av2 = select_argument(i, aa2, sampled_color, accumulator, color_at_stage[i], lighting_colors_e);
            float4 rc = combine_value(cop, cv1, cv2, sampled_color.a);
            float4 ra = combine_value(aop, av1, av2, sampled_color.a);
            if (dst == 1)
                color_at_stage[i] = float4(rc.rgb, ra.a);
            else
                accumulator = float4(rc.rgb, ra.a);
        }
        color = accumulator + lighting_colors[2];
    }
    else
    {
        color = (color - lighting_colors[2]) * texture0.Sample(sampler0, input.texcoord0);
        color += lighting_colors[2];
    }

    if ((alpha_flags & AFLG_ALPHATESTEN) && !alpha_test(color.a))
    {
        discard;
    }
    return color;
}