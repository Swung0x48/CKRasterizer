struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

cbuffer VSCBuf: register(b0)
{
    matrix world_mat;
    matrix view_mat;
    matrix proj_mat;
    matrix total_mat;
    matrix viewport_mat;
    matrix invworld_mat;
    matrix invworldview_mat;
    dword fvf;
};

float4 transform_pos(float3 pos) {
    float4 ret = float4(pos, 1.);
    ret = mul(ret, world_mat);
    ret = mul(ret, view_mat);
    ret = mul(ret, proj_mat);
    return ret;
}