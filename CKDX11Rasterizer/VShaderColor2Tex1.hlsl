#include "Common.hlsl"

struct VS_INPUT
{
    float3 position : SV_POSITION;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 texcoord0 : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = transform_pos(input.position);
    output.worldpos = (float3)mul(float4(input.position, 1.), world_mat);
    output.color = input.diffuse;
    output.specular = input.specular;
    output.normal = float3(0., 0., 0.);
    output.texcoord0 = texgen(input.texcoord0, input.position, 0);
    output.texcoord1 = tex_default;
    return output;
}