#include "Common.hlsl"

float4 main(VS_OUTPUT input) : SV_TARGET
{
    return float4(input.normal, 1.0);
}