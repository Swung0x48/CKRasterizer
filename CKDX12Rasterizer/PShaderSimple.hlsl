#include "Common.hlsl"

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);
Texture2D texture1 : register(t1);
SamplerState sampler1 : register(s1);

float4 main(VS_OUTPUT input) : SV_TARGET
{
    return texture0.Sample(sampler0, input.texcoord0);
    //return float4(sin(input.texcoord0.x), cos(input.texcoord0.y), sin(input.texcoord0.y), 1);
}