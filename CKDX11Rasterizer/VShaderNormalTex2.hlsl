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
    output.worldpos = (float3)mul(float4(input.position, 1.), world_mat);
    output.normal = mul(float4(input.normal, 1.), transposedinvworld_mat).xyz;
    output.color = color_default;
    output.specular = spec_default;
    output.texcoord0 = texgen_normal(input.texcoord0, input.position, input.normal, 0);
    output.texcoord1 = texgen_normal(input.texcoord1, input.position, input.normal, 1);
    return output;
}