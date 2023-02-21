#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.color = color_default;
    output.specular = spec_default;
    output.normal = float3(0., 0., 0.);
    output.texcoord = input.texcoord;
    return output;
}