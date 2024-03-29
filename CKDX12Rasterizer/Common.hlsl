struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 worldpos : POSITION0;
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
    dword _padding1;
    dword _padding2;
    dword _padding3;
    uint4 texture_transform_flags;
    matrix texture_transform_mat[2];
    // matrix texture_transform_mat1;
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

float2 texgen(float2 texcoord, float3 position, int stage)
{
    float4 tc;
    dword texp = texture_transform_flags[stage];
    if (texp & TVP_TC_CSVECP)
        tc = float4(position, 1.);
    else
        tc = float4(texcoord, 0., 0.);
    
    if (texp & TVP_TC_TRANSF)
    {
        // if (stage == 0)
        tc = mul(tc, texture_transform_mat[stage]);
        // else
            // tc = mul(tc, texture_transform_mat1);
    }
    if (texp & TVP_TC_PROJECTED)
        tc /= tc.w;
    
    return tc.xy;
}

float2 texgen_normal(float2 texcoord, float3 position, float3 normal, int stage)
{
    // return texcoord;
    float4 tc = float4(0, 0, 0, 0);
    float4 ffnormal = mul(float4(normal, 1.), transposedinvworldview_mat);
    ffnormal.w = 1.;
    dword texp = texture_transform_flags[stage];
    if ((texp & TVP_TC_CSNORM) != 0U)
        tc = ffnormal;
    else if ((texp & TVP_TC_CSVECP) != 0U)
        tc = float4(position, 1.0);
    else if ((texp & TVP_TC_CSREFV) != 0U)
    {
        float4 pos = float4(position, 1.);
        float4 p = mul(mul(pos, world_mat), view_mat);
        
        tc = float4(reflect(normalize(p.xyz), ffnormal.xyz), 1.);
    }
    else
        tc = float4(texcoord, 0., 1.);
    
    if ((texp & TVP_TC_TRANSF) != 0U)
    {
        tc = mul(tc, texture_transform_mat[stage]);
    }
    if ((texp & TVP_TC_PROJECTED) != 0U)
        tc /= tc.w;
    return tc.xy;
}