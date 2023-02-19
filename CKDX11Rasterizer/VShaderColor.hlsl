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
    // output.color = float4(texcoord, 1.0, 1.0);
    output.texcoord = input.texcoord0;
    return output;
}