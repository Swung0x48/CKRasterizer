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
    norpth = texture(norpth_in, texcoords);
    vec3 posnormal = (norpth.xyz + vec3(1.)) / 2;
    color = vec4(posnormal, 1.);
    gl_FragDepth = norpth.w;
}
