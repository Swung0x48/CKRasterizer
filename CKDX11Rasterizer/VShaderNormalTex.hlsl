#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD;
    float2 texcoord1 : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.normal = mul(input.normal, transposedinvworld_mat);
    output.color = color_default;
    output.specular = spec_default;
    output.texcoord0 = input.texcoord0;
    if (vs_fvf & VF_TEX2)
        output.texcoord1 = input.texcoord1;
    else
        output.texcoord1 = tex_default;
    return output;
}