#version 330 core
#define VERTEX_IS_TRANSFORMED 0 ///placeholder >_<
#define VERTEX_HAS_COLOR      0 ///placeholder >_<
#define HAS_MULTI_TEXTURE     0 ///placeholder >_<

const uint TVP_TC_CSNORM     = 0x01000000U;
const uint TVP_TC_CSVECP     = 0x02000000U;
const uint TVP_TC_CSREFV     = 0x04000000U;
const uint TVP_TC_TRANSF     = 0x08000000U;
const uint TVP_TC_PROJECTED  = 0x00800000U;

layout (location=0) in vec4 xyzw;
layout (location=1) in vec3 normal;
layout (location=2) in vec4 color;
layout (location=3) in vec4 spec_color;
layout (location=4) in vec2 texcoord[8];
out vec3 fpos;
out vec3 fnormal;
out vec4 fragcol;
out vec4 fragscol;
flat out uint fntex;
out vec2 ftexcoord[8];
uniform uint ntex;
uniform mat4 mvp2d;
uniform mat4 world;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 tiworld;
uniform mat4 tiworldview;
uniform mat4 textr[8];
uniform uint texp[8];

void main()
{
    vec4 pos = xyzw;
#if VERTEX_IS_TRANSFORMED
    pos = vec4(xyzw.x, -xyzw.y, xyzw.w, 1.0);
    gl_Position = mvp2d * pos;
#else
    pos = vec4(xyzw.xyz, 1.0);
    gl_Position = proj * view * world * pos;
#endif
    fpos = vec3(world * pos);
    fnormal = mat3(tiworld) * normal;
    vec3 ffnormal = mat3(tiworldview) * normal;
#if VERTEX_HAS_COLOR
    fragcol.rgba = color.bgra; //convert from D3D color BGRA (ARGB as little endian) -> RGBA
    fragscol.rgba = spec_color.bgra;
#else
    fragcol = vec4(1.);
    fragscol = vec4(0.);
#endif
    for (uint i = 0u; i < ntex; ++i)
    {
        vec4 tcout = vec4(0.);
        if ((texp[i] & TVP_TC_CSNORM) != 0U)
            tcout = vec4(ffnormal, 1.);
        else if ((texp[i] & TVP_TC_CSVECP) != 0U)
            tcout = pos;
        else if ((texp[i] & TVP_TC_CSREFV) != 0U)
            tcout = vec4(reflect(normalize((view * world * pos).xyz), ffnormal), 1.);
        else tcout = vec4(texcoord[i], 0., 0.);
        if ((texp[i] & TVP_TC_TRANSF) != 0U)
            tcout = textr[i] * tcout;
        if ((texp[i] & TVP_TC_PROJECTED) != 0U)
            tcout /= tcout.w;
        ftexcoord[i] = vec2(tcout.x, 1 - tcout.y);
    }
    fntex = ntex;
}
