#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR0;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.color = input.diffuse;
    output.specular = spec_default;
    output.normal = float3(0., 0., 0.);
    output.texcoord0 = texgen(input.texcoord0, input.position, 0);
    output.texcoord1 = texgen(input.texcoord1, input.position, 1);
    return output;
}