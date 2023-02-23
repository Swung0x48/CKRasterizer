#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.color = input.diffuse;
    output.specular = input.specular;
    output.normal = float3(0., 0., 0.);
    output.texcoord0 = input.texcoord0;
    output.texcoord1 = input.texcoord1;
    return output;
}