struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

cbuffer VSCBuf: register(b0)
{
    matrix total_mat;
    matrix viewport_mat;
    dword fvf;
};