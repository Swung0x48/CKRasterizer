#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|lineres|f32v2
uniform_parameter|curvature|f32v2
\*  END POST STAGE CONFIGURATION  */
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform vec2 lineres;
uniform vec2 curvature;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
const float PI = 3.14159265358;
vec2 crtcurvature(vec2 uv)
{
    uv = uv * 2. - 1.;
    vec2 d = abs(uv.yx) / vec2(curvature.x, curvature.y);
    uv += uv * d * d;
    return uv * .5 + .5;
}
float sinp(float x, float p)
{
    return pow(abs(sin(x)), p) * sign(sin(x));
}
float hscanfactor(float y, float res)
{
    float f = sin(y * res * PI * 2.0);
    return (f + 1) / 2;
}
vec3 vscanfactor(float x, float res)
{
    float f = sin(x * res * PI * 2.0);
    f = (f + 1) / 2;
    vec3 cmsk = vec3(0.);
    float cm = fract((x * res) / 3);
    if (cm < 1. / 3)
        cmsk.r = 1.;
    else if (cm < 2. / 3)
        cmsk.g = 1.;
    else cmsk.b = 1.;
    return vec3(f, f, f) * cmsk;
}
void main()
{
    vec2 texel_size = 1. / screen_size;
    vec2 disttc = crtcurvature(texcoords);
    vec3 c;
    if (disttc.x < 0 || disttc.y < 0 || disttc.x >= 1. || disttc.y >= 1.)
        discard;
    else
        c = texture(color_in, disttc).rgb;
    float hsc = hscanfactor(disttc.y, lineres.y);
    /*if (false)
    {
        int ms = 20;
        hsc = 0;
        for (int i = -ms; i <= ms; ++i)
        {
            vec2 dtcu = crtcurvature(vec2(texcoords.x + float(i) * texel_size.x, texcoords.y));
            hsc += hscanfactor(dtcu.y, lineres.y);
        }
        hsc /= (float(ms) * 2 + 1);
    }*/
    c *= hsc;
    
    vec3 vsc = vscanfactor(disttc.x, lineres.x);
    if (true)
    {
        int ms = 20;
        vsc = vec3(0.);
        for (int i = -ms; i <= ms; ++i)
        {
            vec2 dtc = crtcurvature(vec2(texcoords.x, texcoords.y + float(i) * texel_size.y));
            vsc += vscanfactor(dtc.x, lineres.x);
        }
        vsc /= (float(ms) * 2 + 1);
    }
    c *= vsc;
    c = pow(c, vec3(0.3));
    color = vec4(c, 1.);
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}

