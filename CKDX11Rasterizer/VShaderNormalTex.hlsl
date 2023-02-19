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
    // output.normal = float4(texcoord, 1.0, 1.0);
    output.texcoord = input.texcoord0;
    return output;
}