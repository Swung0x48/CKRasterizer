#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position, 1.0);
    output.position = mul(output.position, world_mat);
    output.position = mul(output.position, view_mat);
    output.position = mul(output.position, proj_mat);
    // output.color = float4(1.0, 1.0, 1.0, 1.0);
    output.texcoord = float2(0.0, 0.0);
    return output;
}