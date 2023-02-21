#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR;
    float4 specular : COLOR;
    float2 texcoord0 : TEXCOORD;
    float2 texcoord1 : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.color = input.diffuse;
    output.specular = input.specular;
    output.normal = float3(0., 0., 0.);
    output.texcoord0 = input.texcoord0;
    if (vs_fvf & VF_TEX2)
        output.texcoord1 = input.texcoord1;
    else
        output.texcoord1 = tex_default;
    return output;
}