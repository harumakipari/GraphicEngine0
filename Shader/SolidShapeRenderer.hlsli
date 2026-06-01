struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

cbuffer CbMesh : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 viewProjection;
    float4 lightDirection;
    float4 materialColor;
};
