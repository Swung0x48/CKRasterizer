#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float2 texcoord1 : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 pos4 = float4(input.position, 1.0);
    output.position = mul(pos4, total_mat);
    // output.normal = float4(texcoord, 1.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}