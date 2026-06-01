#include "GltfModel.hlsli"

struct ClothVertex
{
    float4 position;
    float4 normal;
    float4 tangent;
    float4 texcoord;
    
    float4 oldVelocity;
    float4 velocity;
    float4 newPosition;
};

StructuredBuffer<ClothVertex> clothVertices : register(t0);


VS_OUT main(uint vertexId : SV_VertexID)
{
    VS_OUT vout;
    ClothVertex vin = clothVertices[vertexId];
    float sigma = vin.tangent.w;
    vout.position = mul(vin.position, mul(world, viewProjection));
    
    //vout.position /= vout.position.w; // to ndc
    
    vout.wPosition = mul(vin.position, world);
    //vin.normal.w = 0;
    
    if (vout.wPosition.y < 0.0)
    {
        vout.wPosition.y = 0.0;
    }
    
    vout.wNormal = normalize(mul(vin.normal, world));
    
    vin.tangent.w = 0;
    vout.wTangent = normalize(mul(vin.tangent, world));
    vout.wTangent.w = sigma;
    
    vout.texcoord = vin.texcoord;

    return vout;
}