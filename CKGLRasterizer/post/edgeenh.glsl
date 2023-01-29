#version 330 core
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
void main()
{
    vec2 texel_size = 1. / screen_size;
    vec2 d[4] = vec2[4](vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1));
    vec4 cc = texture(color_in, texcoords);
    vec3 nc = normalize(cc.xyz);
    float ddc[4];
    for (int i = 0; i < 4; ++ i)
    {
        vec3 ndc = normalize(texture(color_in, texcoords + d[i] * texel_size).xyz);
        ddc[i] = dot(nc, ndc);
    }
    float sc = 64;
    float f = 1.;
    f += sc * (ddc[0] - ddc[1]);
    f += sc * (ddc[2] - ddc[3]);
    color = cc * clamp(f, .5, 2);
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}
