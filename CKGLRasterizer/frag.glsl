#version 330 core
#define DEBUG
const uint LSW_SPECULAR_ENABLED = 0x0001U;
const uint LSW_LIGHTING_ENABLED = 0x0002U;
const uint LSW_VRTCOLOR_ENABLED = 0x0004U;
const uint LSW_SPCL_OVERR_FORCE = 0x0008U;
const uint LSW_SPCL_OVERR_ONLY  = 0x0010U;

struct mat_t
{
    vec4 ambi;          //@0
    vec4 diff;          //@16
    vec4 spcl;          //@32
    float spcl_strength;//@48 + 12 padding
    vec4 emis;          //@64
};
struct light_t
{
    //1=point, 2=spot, 3=directional
    uint type;          //@0 + 12 padding
    vec4 ambi;          //@16
    vec4 diff;          //@32
    vec4 spcl;          //@48
    //directional
    vec4 dir;           //@64
    //point & spot
    vec4 pos;           //@80
    vec4 psparam1;      //@96, xyzw=range, falloff, theta, phi
    vec4 psparam2;      //@112, xyzw=a0, a1, a2
    //stride per element: 128
};
struct texcomb_t
{
    uint op;                //@0, bit 0-3: color op, bit 4-7: alpha op, bit 31: dest
    uint cargs;             //@4, bit 0-7: arg1, bit 8-15: arg2, bit 16-23: arg3
    uint aargs;             //@8, ditto but for alpha
    uint constant;          //@12
    //stride per element: 16
};

in vec3 fpos;
in vec3 fnormal;
in vec4 fragcol;
in vec4 fragscol;
flat in uint fntex;
in vec2 ftexcoord[8];
uniform float alpha_thresh;
uniform uint alphatest_flags;
uniform uint fog_flags;
uniform vec4 fog_color;
uniform vec3 fog_parameters; //start, end, density
uniform vec3 vpos; //camera position
uniform vec2 depth_range; //near-far plane distances for fog calculation
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
layout (std140) uniform MatUniformBlock
{
    mat_t material;
};
uniform uint lighting_switches;
layout (std140) uniform LightsUniformBlock
{
    light_t lights[16]; // this is 2 KiB already...
};
layout (std140) uniform TexCombinatorUniformBlock
{
    texcomb_t texcomb[8];
};
uniform sampler2D tex[8];

vec4[3] light_point(light_t l, vec3 normal, vec3 fpos, vec3 vdir, bool spec_enabled)
{
    bool use_vert_color = (lighting_switches & LSW_VRTCOLOR_ENABLED) != 0U;
    float range = l.psparam1.x;
    float a0 = l.psparam2.x;
    float a1 = l.psparam2.y;
    float a2 = l.psparam2.z;
    vec3 ldir = normalize(l.pos.xyz - fpos);
    float dist = length(l.pos.xyz - fpos);
    if (dist > range) return vec4[3](vec4(0.), vec4(0.), vec4(0.));
    //CK2_3D always gives us DX5 light parameters... thus the DX5 light formula here.
    dist = 1. - dist / range;
    float atnf = a0 + (a1 * dist + a2 * dist * dist);
    float diff = max(dot(normal, ldir), 0.);
    vec4 amb = l.ambi * material.ambi;
    vec4 dif = diff * l.diff * material.diff;
    if (use_vert_color)
        dif = diff * l.diff * fragcol;
    vec4 spc = vec4(0.);
    if (spec_enabled
#ifdef DEBUG
    && (lighting_switches & LSW_SPCL_OVERR_FORCE) == 0U
#endif
    )
    {
        vec3 refldir = reflect(-ldir, normal);
        float specl = pow(clamp(dot(vdir, refldir), 0., 1.), material.spcl_strength);
        spc = l.spcl * material.spcl * specl;
        if (use_vert_color)
            spc = l.spcl * fragscol * specl;
#ifdef DEBUG
        if ((lighting_switches & LSW_SPCL_OVERR_ONLY) != 0U)
            return vec4[3](vec4(0.), vec4(0.), spc * atnf);
#endif
    }
    return vec4[3](amb * atnf, dif * atnf, spc * atnf);
}
vec4[3] light_directional(light_t l, vec3 normal, vec3 vdir, bool spec_enabled)
{
    bool use_vert_color = (lighting_switches & LSW_VRTCOLOR_ENABLED) != 0U;
    vec3 ldir = normalize(-l.dir.xyz);
    float diff = max(dot(normal, ldir), 0.);
    vec4 amb = l.ambi * material.ambi;
    vec4 dif = diff * l.diff * material.diff;
    if (use_vert_color)
        dif = diff * l.diff * fragcol;
    vec4 spc = vec4(0.);
    if (spec_enabled
#ifdef DEBUG
    && (lighting_switches & LSW_SPCL_OVERR_FORCE) == 0U
#endif
    )
    {
        vec3 refldir = reflect(-ldir, normal);
        float specl = pow(clamp(dot(vdir, refldir), 0., 1.), material.spcl_strength);
        //direct3d9 specular strength formula
        //vec3 hv = normalize(normalize(vpos - fpos) + ldir);
        //specl = pow(dot(normal, hv), material.spcl_strength);
        spc = l.spcl * material.spcl * specl;
        if (use_vert_color)
            spc = l.spcl * fragscol * specl;
#ifdef DEBUG
        if ((lighting_switches & LSW_SPCL_OVERR_ONLY) != 0U)
            return vec4[3](vec4(0), vec4(0), spc);
#endif
    }
    return vec4[3](amb, dif, spc);
}
bool alpha_test(float in_alpha)
{
    switch(alphatest_flags & 0xFU)
    {
        case 1U: return false;
        case 2U: return in_alpha <  alpha_thresh;
        case 3U: return in_alpha == alpha_thresh;
        case 4U: return in_alpha <= alpha_thresh;
        case 5U: return in_alpha >  alpha_thresh;
        case 6U: return in_alpha != alpha_thresh;
        case 7U: return in_alpha >= alpha_thresh;
        default: return true;
    }
}
vec4 dw2color(uint c)
{
    return vec4(
        float((c >> 16U) & 0xFFU) / 255.,
        float((c >>  8U) & 0xFFU) / 255.,
        float((c >>  0U) & 0xFFU) / 255.,
        float((c >> 24U) & 0xFFU) / 255.);
}
vec4 clamp_color(vec4 c)
{
    return clamp(c, vec4(0, 0, 0, 0), vec4(1, 1, 1, 1));
}
vec4[3] component_add(vec4[3] a, vec4[3] b)
{
    return vec4[3](a[0] + b[0], a[1] + b[1], a[2] + b[2]);
}
vec4 accum_light(vec4[3] c)
{
    return c[0] + c[1] + c[2];
}
vec4 accum_light_e(vec4[4] c)
{
    return c[0] + c[1] + c[2] + c[3];
}
float fog_factor(float dist, float rdist, uint mode)
{
    switch(mode)
    {
        case 1U: return 1. / exp(dist * fog_parameters.z);
        case 2U: return 1. / exp(pow(dist * fog_parameters.z, 2));
        case 3U: return (fog_parameters.y - rdist) / (fog_parameters.y - fog_parameters.x);
        default: return 1.;
    }
}
vec4 combine_value(uint mode, vec4 a, vec4 b, float factor)
{
    switch(mode)
    {
        case  2U: return a;
        case  3U: return b;
        case  4U: return a * b;
        case  5U: return 2 * (a * b);
        case  6U: return 4 * (a * b);
        case  7U: return a + b;
        case  8U: return a + b - vec4(0.5);
        case  9U: return 2 * (a + b - vec4(0.5));
        case 10U: return a - b;
        case 11U: return a + b - a * b;
        case 13U: return mix(b, a, factor);
        default: return vec4(0.);
    }
}
vec4 select_argument(uint stage, uint source, vec4 tex, vec4 cum, vec4 tmp, vec4[4] lights)
{
    vec4 ret = vec4(0.);
    switch(source) //D3DTA_*
    {
        case 0U: ret = lights[1]; break; //DIFFUSE
        case 1U: if (stage == 0U)        //CURRENT
                    ret = accum_light_e(lights);
                else ret = cum; break;
        case 2U: ret = tex; break;       //TEXTURE
        case 3U: ret = vec4(0.); break;  //TFACTOR, unsupported
        case 4U: ret = lights[2]; break; //SPECULAR
        case 5U: ret = tmp; break;       //TEMP
        case 6U: ret = dw2color(texcomb[stage].constant); break; //CONSTANT
    }
    if ((source & 0x10U) != 0U)
        ret = vec4(1.) - ret;
    if ((source & 0x20U) != 0U)
        ret = vec4(ret.a);
    return ret;
}
void main()
{
    vec3 norm = normalize(fnormal);
    vec3 vdir = normalize(vpos - fpos);
    norpth = vec4(norm, gl_FragCoord.z);

    float ffactor = 1.;
    if ((fog_flags & 0x80U) != 0U)
    {
        float fvdepth = clamp(length(vpos - fpos) / (depth_range.y - depth_range.x), 0, 1);
        float rdepth = length(vpos - fpos);
        ffactor = fog_factor(fvdepth, rdepth, fog_flags & 0x0fU);
        if (ffactor < 1. / 512)
        {
            color = fog_color;
            return;
        }
    }

    color = vec4(1., 1., 1., 1.);
    vec4[3] lighting_colors = vec4[3](vec4(0.), vec4(0.), vec4(0.));
    vec4[4] lighting_colors_e = vec4[4](vec4(0.), vec4(0.), vec4(0.), vec4(0.));
    if ((lighting_switches & LSW_LIGHTING_ENABLED) != 0U)
    {
        for (uint i = 0U; i < 16U; ++i)
        {
            switch (lights[i].type)
            {
                case 1U: lighting_colors = component_add(
                    lighting_colors,
                    light_point(lights[i], norm, fpos, vdir, (lighting_switches & LSW_SPECULAR_ENABLED) != 0U));
                    break;
                case 3U: lighting_colors = component_add(
                    lighting_colors,
                    light_directional(lights[i], norm, vdir, (lighting_switches & LSW_SPECULAR_ENABLED) != 0U));
                    break;
            }
        }
        lighting_colors[0] = clamp_color(lighting_colors[0]);
        lighting_colors[1] = clamp_color(lighting_colors[1]);
        lighting_colors[2] = clamp_color(lighting_colors[2]);
        color = accum_light(lighting_colors);
        lighting_colors_e = vec4[4](lighting_colors[0], lighting_colors[1], lighting_colors[2], vec4(0.));
#ifdef DEBUG
        if ((lighting_switches & LSW_SPECULAR_ENABLED) == 0U ||
            (lighting_switches & LSW_SPECULAR_ENABLED) != 0U && (lighting_switches & LSW_SPCL_OVERR_ONLY) == 0U)
#endif
        {
            lighting_colors_e[3] = material.emis;
            color = accum_light_e(lighting_colors_e);
            color.a = clamp(color.a, 0, 1);
            if (material.diff.a < 1)
                color.a = material.diff.a;
        }
    } else color = fragcol;
    if (fntex > 1U)
    {
        vec4 accum = vec4(0.);
        vec4 temp[8] = vec4[8](vec4(0.), vec4(0.), vec4(0.), vec4(0.),
                               vec4(0.), vec4(0.), vec4(0.), vec4(0.));
        for (uint i = 0U; i < fntex; ++i)
        {
            vec4 txc = vec4(0.);
            switch (i)
            {
                case 0U: txc = texture(tex[0], ftexcoord[i]); break;
                case 1U: txc = texture(tex[1], ftexcoord[i]); break;
                case 2U: txc = texture(tex[2], ftexcoord[i]); break;
                case 3U: txc = texture(tex[3], ftexcoord[i]); break;
                case 4U: txc = texture(tex[4], ftexcoord[i]); break;
                case 5U: txc = texture(tex[5], ftexcoord[i]); break;
                case 6U: txc = texture(tex[6], ftexcoord[i]); break;
                case 7U: txc = texture(tex[7], ftexcoord[i]); break;
            }
            uint cop = texcomb[i].op & 0xfU;
            uint aop = (texcomb[i].op & 0xf0U) >> 4;
            uint dst = texcomb[i].op >> 31; //1: temp, 0: accumulator
            uint ca1 = texcomb[i].cargs & 0xffU;
            uint ca2 = (texcomb[i].cargs & 0xff00U) >> 8;
            uint aa1 = texcomb[i].aargs & 0xffU;
            uint aa2 = (texcomb[i].aargs & 0xff00U) >> 8;
            vec4 cv1 = select_argument(i, ca1, txc, accum, temp[i], lighting_colors_e);
            vec4 cv2 = select_argument(i, ca2, txc, accum, temp[i], lighting_colors_e);
            vec4 av1 = select_argument(i, aa1, txc, accum, temp[i], lighting_colors_e);
            vec4 av2 = select_argument(i, aa2, txc, accum, temp[i], lighting_colors_e);
            vec4 rc = combine_value(cop, cv1, cv2, txc.a);
            vec4 ra = combine_value(aop, av1, av2, txc.a);
            if (dst == 1U)
                temp[i] = vec4(rc.rgb, ra.a);
            else
                accum = vec4(rc.rgb, ra.a);
        }
        color = accum;
    }
    else
    {
        if ((texcomb[0].op & 0xfU) == 13U) //This doesn't make ANY sense
            color = (max(lighting_colors_e[0] + lighting_colors_e[1] + lighting_colors_e[3], vec4(1.)) + lighting_colors_e[2]) * texture(tex[0],ftexcoord[0]);
        else
            color *= texture(tex[0], ftexcoord[0]);
    }
    if ((alphatest_flags & 0x80U) != 0U && !alpha_test(color.a))
        discard;
    color = mix(fog_color, clamp_color(color), ffactor);
}
