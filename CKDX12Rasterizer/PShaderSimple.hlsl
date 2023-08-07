#include "Common.hlsl"

float4 main(VS_OUTPUT input) : SV_TARGET
{
    return float4(sin(input.texcoord0.x), cos(input.texcoord0.y), sin(input.texcoord0.y), 1);
}