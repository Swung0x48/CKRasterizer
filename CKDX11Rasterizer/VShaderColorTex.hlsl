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
    float4 pos4 = float4(input.position, 1.0);
    output.position = mul(pos4, total_mat);
    output.texcoord = input.texcoord0;
    return output;
}