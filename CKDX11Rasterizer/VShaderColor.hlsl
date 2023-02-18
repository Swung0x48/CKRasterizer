#include "Common.hlsl"

VS_OUTPUT main(float4 position : SV_POSITION, float4 color : COLOR, float2 texcoord : TEXCOORD)
{
    VS_OUTPUT output;
    output.position = float4(position.x, -position.y, position.w, 1.0);
    output.position = mul(viewport_mat, output.position);
    // output.color = float4(texcoord, 1.0, 1.0);
    output.texcoord = texcoord;
    return output;
}