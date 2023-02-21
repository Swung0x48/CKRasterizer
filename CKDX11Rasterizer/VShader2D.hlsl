#include "Common.hlsl"

struct VS_INPUT
{
    float4 position : SV_POSITION;
    float4 diffuse : COLOR;
    float4 specular : COLOR;
    float2 texcoord0 : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position.x, -input.position.y, input.position.w, 1.0);
    output.position = mul(viewport_mat, output.position);
    output.normal = float3(0., 0., 0.);
    output.color = input.diffuse;
    output.specular = input.specular;
    output.texcoord0 = input.texcoord0;
    return output;
}