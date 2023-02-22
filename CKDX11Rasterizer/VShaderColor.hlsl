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
    output.color = input.diffuse;
    output.specular = spec_default;
    output.normal = float3(0., 0., 0.);
    output.texcoord0 = tex_default;
    output.texcoord1 = tex_default;
    return output;
}