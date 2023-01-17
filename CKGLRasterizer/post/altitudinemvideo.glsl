#version 330 core
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D normal_in;
uniform sampler2D depth_in;
in vec2 texcoords;
out vec4 color;
out vec3 normal;
void main()
{
    color = vec4(vec3(texture(depth_in, texcoords).x), 1.);
    normal = texture(normal_in, texcoords).xyz;
    gl_FragDepth = texture(depth_in, texcoords).x;
}
