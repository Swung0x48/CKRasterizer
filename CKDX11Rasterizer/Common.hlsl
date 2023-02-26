struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float4 specular : COLOR1;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
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
    dword vs_fvf;
    dword texture_transform_flags[2];
};

static const float4 color_default = float4(1., 1., 1., 1.);
static const float4 spec_default = float4(1., 1., 1., 1.);
static const float2 tex_default = float2(0., 0.);

static const dword TVP_TC_CSNORM = 0x01000000; // use camera space normal as input tex-coords
static const dword TVP_TC_CSVECP = 0x02000000; // use camera space position ......
static const dword TVP_TC_CSREFV = 0x04000000; // use camera space reflect vector ......
static const dword TVP_TC_TRANSF = 0x08000000; // tex-coords should be transformed by its matrix
static const dword TVP_TC_PROJECTED = 0x10000000; // tex-coords should be projected

static const dword VF_TEX2 = 0x0200;

float4 transform_pos(float3 pos) {
    float4 ret = float4(pos, 1.);
    ret = mul(ret, world_mat);
    ret = mul(ret, view_mat);
    ret = mul(ret, proj_mat);
    return ret;
}