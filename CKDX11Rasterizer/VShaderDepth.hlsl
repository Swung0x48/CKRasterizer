struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT main(uint VertID : SV_VertexID)
{
    VS_OUTPUT output;
    output.UV = float2((VertID << 1) & 2, VertID & 2);
    output.position = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
    output.position.y = -output.position.y;
    return output;
}