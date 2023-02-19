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
    output.normal = mul(input.normal, invworld_mat);
    output.color = float4(1., 1., 1., 1.);
    output.texcoord = input.texcoord0;
    return output;
}