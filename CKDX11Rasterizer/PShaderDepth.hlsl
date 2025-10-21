struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D g_DepthTexture : register(t0);
SamplerState g_SamplerPoint : register(s0);

float4 main(VS_OUTPUT In) : SV_TARGET
{
    float depth = g_DepthTexture.Sample(g_SamplerPoint, In.UV).r;
    
    return float4(depth, 0, 0, 1);
}