#version 330 core
const uint VP_HAS_COLOR      = 0x10000000U;
const uint VP_IS_TRANSFORMED = 0x20000000U;
const uint VP_TEXTURE_MASK   = 0x0000000fU;

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
flat out uint fntex;
out vec2 ftexcoord[8];
uniform uint vertex_properties;
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
    bool is_transformed = (vertex_properties & VP_IS_TRANSFORMED) != 0U;
    bool has_color = (vertex_properties & VP_HAS_COLOR) != 0U;
    uint ntex = (vertex_properties & VP_TEXTURE_MASK);
    vec4 pos = xyzw;
    if (is_transformed)
    {
        pos = vec4(xyzw.x, -xyzw.y, xyzw.w, 1.0);
        gl_Position = mvp2d * pos;
    }
    else
    {
        pos = vec4(xyzw.xyz, 1.0);
        gl_Position = proj * view * world * pos;
    }
    fpos = vec3(world * pos);
    fnormal = mat3(tiworld) * normal;
    vec3 ffnormal = mat3(tiworldview) * normal;
    if (has_color)
        fragcol.rgba = color.bgra; //convert from D3D color BGRA (ARGB as little endian) -> RGBA
    else fragcol = vec4(1., 1., 1., 1.);
    for (uint i = 0u; i < ntex; ++i)
    {
        vec4 tcout = vec4(0.);
        if ((texp[i] & TVP_TC_CSNORM) != 0U)
            tcout = vec4(ffnormal, 1.);
        else if ((texp[i] & TVP_TC_CSVECP) != 0U)
            tcout = pos;
        else if ((texp[i] & TVP_TC_CSREFV) != 0U)
            tcout = vec4(reflect(normalize(pos.xyz), ffnormal), 1.);
        else tcout = vec4(texcoord[i], 0., 0.);
        if ((texp[i] & TVP_TC_TRANSF) != 0U)
            tcout = textr[i] * tcout;
        if ((texp[i] & TVP_TC_PROJECTED) != 0U)
            tcout /= tcout.w;
        ftexcoord[i] = vec2(tcout.x, 1 - tcout.y);
    }
    fntex = ntex;
}
