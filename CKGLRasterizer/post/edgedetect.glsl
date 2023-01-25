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
    vec4 edge_color = vec4(.8, 0., 1., 1.);
    vec2 texel_size = 1. / screen_size;
    norpth = texture(norpth_in, texcoords);
    vec4 avg = vec4(0.);
    vec2 d[8] = vec2[8](vec2(-1, -1), vec2(-1, 0), vec2(-1, 1),
                        vec2( 0, -1),              vec2( 0, 1),
                        vec2( 1, -1), vec2( 1, 0), vec2( 1, 1));
    for (int i = 0; i < 8; ++i)
        avg += texture(norpth_in, texcoords + d[i] * texel_size);
    avg /= 8;
    float thresh = 0.01;
    float depth_thresh = 1. - 1. / 84.;
    color = mix(vec4(0.), edge_color, step(thresh, length(norpth - avg)));
    color = mix(color, vec4(0.), step(depth_thresh, norpth.w));
    color.a = 1.;
    gl_FragDepth = norpth.w;
}
