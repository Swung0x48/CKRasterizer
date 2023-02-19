#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    // output.color = float4(1.0, 1.0, 1.0, 1.0);
    output.texcoord = float2(0., 0.);
    return output;
}