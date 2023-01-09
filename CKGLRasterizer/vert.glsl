#version 330 core
layout (location=0) in vec4 xyzw;
layout (location=1) in vec3 normal;
layout (location=2) in vec4 color;
layout (location=3) in vec4 spec_color;
layout (location=4) in vec2 texcoord;
out vec3 fpos;
out vec3 fnormal;
out vec4 fragcol;
out vec2 ftexcoord;
uniform bool is_transformed;
uniform bool has_color;
uniform mat4 world;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 tiworld;
void main()
{
    vec4 pos = xyzw;
    if (!is_transformed) pos = vec4(xyzw.xyz, 1.0);
    gl_Position = proj * view * world * pos;
    fpos = vec3(world * pos);
    fnormal = mat3(tiworld) * normal;
    if (has_color)
        fragcol.rgba = color.bgra; //convert from D3D color BGRA (ARGB as little endian) -> RGBA
    else fragcol = vec4(1., 1., 1., 1.);
    ftexcoord = vec2(texcoord.x, 1 - texcoord.y);
}
