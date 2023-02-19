#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position, 1.0);
    output.position = mul(output.position, world_mat);
    output.position = mul(output.position, view_mat);
    output.position = mul(output.position, proj_mat);
    // output.color = float4(texcoord, 1.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}