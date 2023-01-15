#version 330 core
const uint LSW_SPECULAR_ENABLED = 0x0001U;
const uint LSW_LIGHTING_ENABLED = 0x0002U;
const uint LSW_VRTCOLOR_ENABLED = 0x0004U;

struct mat_t
{
    vec4 ambi;          //@0
    vec4 diff;          //@16
    vec4 spcl;          //@32
    float spcl_strength;//@48
    vec4 emis;          //@64
};
struct light_t
{
    //1=point, 2=spot, 3=directional
    uint type;
    vec4 ambi;
    vec4 diff;
    vec4 spcl;
    //directional
    vec4 dir;
    //point & spot
    vec4 pos;
    float range;
    //point
    float a0;
    float a1;
    float a2;
    //spot (cone)
    float falloff;
    float theta;
    float phi;
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
flat in uint fntex;
in vec2 ftexcoord[8];
out vec4 color;
uniform float alpha_thresh;
uniform uint alphatest_flags;
uniform uint fog_flags;
uniform vec4 fog_color;
uniform vec3 fog_parameters; //start, end, density
uniform vec3 vpos; //camera position
uniform vec2 depth_range; //near-far plane distances for fog calculation
layout (std140) uniform MatUniformBlock
{
    mat_t material;
};
uniform uint lighting_switches;
uniform light_t lights; // will become array in the future
layout (std140) uniform TexCombinatorUniformBlock
{
    texcomb_t texcomb[8];
};
uniform sampler2D tex[8]; //this will become an array in the future

vec4[4] light_directional(light_t l, vec3 normal, vec3 vdir, bool spec_enabled)
{
    vec3 ldir = normalize(-l.dir.xyz);
    float diff = max(dot(normal, ldir), 0.);
    vec4 amb = l.ambi * material.ambi;
    vec4 dif = diff * l.diff * material.diff;
    vec4 spc = vec4(0.);
    vec4 ems = material.emis;
    if (spec_enabled)
    {
        vec3 refldir = reflect(-ldir, normal);
        float specl = pow(max(dot(vdir, refldir), 0.), material.spcl_strength);
        spc = l.spcl * material.spcl * specl;
        //return vec4[4](vec4(0), vec4(0), spc, vec4(0));
    }
    return vec4[4](amb, dif, spc, ems);
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
vec4 accum_light(vec4[4] c)
{
    return clamp_color(vec4((c[0] + c[1] + c[2] + c[3]).xyz, 1.));
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
                    ret = accum_light(lights);
                else ret = cum; break;
        case 2U: ret = tex; break;       //TEXTURE
        case 3U: ret = vec4(0.); break;   //TFACTOR
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
    vec4[4] lighting_colors = vec4[4](vec4(0.), vec4(0.), vec4(0.), vec4(0.));
    if ((lighting_switches & LSW_VRTCOLOR_ENABLED) != 0U)
        color = fragcol;
    if (lights.type == uint(3) && (lighting_switches & LSW_LIGHTING_ENABLED) != 0U)
    {
        lighting_colors = light_directional(lights, norm, vdir, (lighting_switches & LSW_SPECULAR_ENABLED) != 0U);
        color = accum_light(lighting_colors);
    }
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
            vec4 cv1 = select_argument(i, ca1, txc, accum, temp[i], lighting_colors);
            vec4 cv2 = select_argument(i, ca2, txc, accum, temp[i], lighting_colors);
            vec4 av1 = select_argument(i, aa1, txc, accum, temp[i], lighting_colors);
            vec4 av2 = select_argument(i, aa2, txc, accum, temp[i], lighting_colors);
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
        color *= texture(tex[0], ftexcoord[0]);
    if ((alphatest_flags & 0x80U) != 0U && !alpha_test(color.a))
        discard;
    color = mix(fog_color, color, ffactor);
}
