#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR;
    float4 specular : COLOR;
    float2 texcoord0 : TEXCOORD;
    float2 texcoord1 : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.color = input.diffuse;
    output.normal = float3(0., 0., 0.);
    output.texcoord = input.texcoord0;
    return output;
}