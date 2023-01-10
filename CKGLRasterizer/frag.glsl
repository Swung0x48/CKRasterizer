#version 330 core
const uint LSW_SPECULAR_ENABLED = 0x0001U;
const uint LSW_LIGHTING_ENABLED = 0x0002U;
const uint LSW_VRTCOLOR_ENABLED = 0x0004U;

struct mat_t
{
    vec3 ambi;
    vec3 diff;
    vec3 spcl;
    float spcl_strength;
    vec3 emis;
};
struct light_t
{
    uint type; //1=point, 2=spot, 3=directional
    vec3 ambi;
    vec3 diff;
    vec3 spcl;
    //directional
    vec3 dir;
    //point & spot
    vec3 pos;
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

in vec3 fpos;
in vec3 fnormal;
in vec4 fragcol;
in vec2 ftexcoord;
out vec4 color;
uniform float alpha_thresh;
uniform uint alphatest_flags;
uniform uint fog_flags;
uniform vec4 fog_color;
uniform vec3 fog_parameters; //start, end, density
uniform vec3 vpos; //camera position
uniform vec2 depth_range; //near-far plane distances for fog calculation
uniform mat_t material;
uniform uint lighting_switches;
uniform light_t lights; // will become array in the future
uniform sampler2D tex; //this will become an array in the future

vec3 light_directional(light_t l, vec3 normal, vec3 vdir, bool spec_enabled)
{
    vec3 ldir = normalize(-l.dir);
    float diff = max(dot(normal, ldir), 0.);
    vec3 ret = vec3(0., 0., 0.);
    vec3 amb = l.ambi * material.ambi;
    vec3 dif = diff * l.diff * material.diff;
    ret = amb + dif + material.emis;
    if (spec_enabled)
    {
        vec3 refldir = reflect(-ldir, normal);
        float specl = pow(max(dot(vdir, refldir), 0.), material.spcl_strength);
        vec3 spc = l.spcl * material.spcl * specl;
        ret += spc;
    }
    return ret;
}
bool alpha_test(float in_alpha)
{
    switch(alphatest_flags & 0xFU)
    {
        case 1: return false;
        case 2: return in_alpha <  alpha_thresh;
        case 3: return in_alpha == alpha_thresh;
        case 4: return in_alpha <= alpha_thresh;
        case 5: return in_alpha >  alpha_thresh;
        case 6: return in_alpha != alpha_thresh;
        case 7: return in_alpha >= alpha_thresh;
        default: return true;
    }
}
vec4 clamp_color(vec4 c)
{
    return clamp(c, vec4(0, 0, 0, 0), vec4(1, 1, 1, 1));
}
float fog_factor(float dist, uint mode)
{
    switch(mode)
    {
        case 1: return 1. / exp(dist * fog_parameters.z);
        case 2: return 1. / exp(pow(dist * fog_parameters.z, 2));
        case 3: return (fog_parameters.y - dist) / (fog_parameters.y - fog_parameters.x);
        default: return 1.;
    }
}
void main()
{
    vec3 norm = normalize(fnormal);
    vec3 vdir = normalize(vpos - fpos);

    float ffactor = 1.;
    if ((fog_flags & 0x80U) != 0U)
    {
        float fvdepth = clamp(length(vpos - fpos) / (depth_range.y - depth_range.x), 0, 1);
        float ffactor = fog_factor(fvdepth, fog_flags & 0x08U);
        if (ffactor < 1. / 512)
        {
            color = fog_color;
            return;
        }
    }

    color = vec4(1., 1., 1., 1.);
    if ((lighting_switches & LSW_VRTCOLOR_ENABLED) != 0U)
        color = fragcol;
    if (lights.type == uint(3) && (lighting_switches & LSW_LIGHTING_ENABLED) != 0U)
        color *= clamp_color(vec4(light_directional(lights, norm, vdir, (lighting_switches & LSW_SPECULAR_ENABLED) != 0U), 1.0));
    color *= texture(tex, ftexcoord);
    if ((alphatest_flags & 0x80U) != 0U && !alpha_test(color.a))
        discard;
    color = mix(fog_color, color, ffactor);
}
