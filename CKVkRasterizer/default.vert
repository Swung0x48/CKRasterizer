#version 450

layout (std140, binding=0) uniform MatricesUniformBlock
{
    mat4 vp2d;
    mat4 world;
    mat4 view;
    mat4 proj;
} m;

layout (push_constant) uniform PushConstants
{
    int flags[4];
    int atexid[8];
} f;

layout (location=0) in vec4 xyzw;
layout (location=1) in vec3 normal;
layout (location=2) in vec4 color;
layout (location=3) in vec4 spec_color;
layout (location=4) in vec2 texcoord[8];

layout (location=0) out vec2 ftc[8];
layout (location=8) out int atexid[8];

void main()
{
    if ((f.flags[0] & 1) != 0)
    {
        vec4 pos = vec4(xyzw.x, -xyzw.y, xyzw.w, 1.0);
        gl_Position = m.vp2d * pos;
    }
    else
        gl_Position = m.proj * m.view * m.world * vec4(xyzw.xyz, 1.0);
    ftc = texcoord;
    atexid = f.atexid;
}
