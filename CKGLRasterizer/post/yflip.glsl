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
    vec2 tc = vec2(texcoords.x, 1. - texcoords.y);
    color = texture(color_in, tc);
    norpth = texture(norpth_in, tc);
    gl_FragDepth = texture(norpth_in, tc).w;
}
