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
    normal = texture(norpth_in, texcoords).xyz;
    vec3 posnormal = (normal + vec3(1.)) / 2;
    color = vec4(posnormal, 1.);
    gl_FragDepth = texture(norpth_in, texcoords).w;
}
