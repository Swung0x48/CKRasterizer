struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

cbuffer CBuf
{
    matrix total_mat;
    matrix viewport_mat;
};

VS_OUTPUT VShader0x42(float3 position : SV_POSITION, float4 diffuse : COLOR)
{
    VS_OUTPUT output;
    float4 pos4 = float4(position, 1.0);
    output.position = mul(pos4, total_mat);
    output.color = float4(1.0, 1.0, 1.0, 1.0);
    output.texcoord = float2(0.0, 0.0);
    return output;
}