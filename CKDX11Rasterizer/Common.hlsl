struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float4 specular : COLOR1;
    float2 texcoord : TEXCOORD;
};

cbuffer VSCBuf: register(b0)
{
    matrix world_mat;
    matrix view_mat;
    matrix proj_mat;
    matrix total_mat;
    matrix viewport_mat;
    matrix transposedinvworld_mat;
    matrix transposedinvworldview_mat;
    dword fvf;
};

static const float4 color_default = float4(1., 1., 1., 1.);
static const float4 spec_default = float4(0., 0., 0., 0.);

float4 transform_pos(float3 pos) {
    float4 ret = float4(pos, 1.);
    ret = mul(ret, world_mat);
    ret = mul(ret, view_mat);
    ret = mul(ret, proj_mat);
    return ret;
}