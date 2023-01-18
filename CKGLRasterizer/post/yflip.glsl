#version 330 core
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
in vec2 texcoords;
out vec4 color;
out vec3 normal;
void main()
{
    vec2 tc = vec2(texcoords.x, 1. - texcoords.y);
    color = texture(color_in, tc);
    normal = texture(norpth_in, tc).xyz;
    gl_FragDepth = texture(norpth_in, tc).w;
}
