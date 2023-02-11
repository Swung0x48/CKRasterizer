Texture2D texture2d;
SamplerState sampler_st;
float4 PShader(float4 position : SV_POSITION, float4 color : COLOR, float2 texcoord : TEXCOORD) : SV_TARGET
{
    return texture2d.Sample(sampler_st, float2(texcoord.x, 1 - texcoord.y));
}