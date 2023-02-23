#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.normal = mul(input.normal, transposedinvworld_mat);
    output.color = color_default;
    output.specular = spec_default;
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord1;
    return output;
}