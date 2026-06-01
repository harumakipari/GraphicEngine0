#include "Constants.hlsli"
#include "GltfModel.hlsli"

struct CsmConstants
{
    row_major float4x4 cascadedMatrices[4];
    float4 cascadedPlaneDistances;
};

cbuffer csmConstants : register(b3)
{
    CsmConstants csmData;
}

// CASCADED_SHADOW_MAPS
struct VS_OUT_CSM
{
    float4 position : SV_POSITION;
    uint instanceId : INSTANCEID;
};

struct GS_OUTPUT_CSM
{
    float4 position : SV_POSITION;
    uint renderTargetArrayIndex : SV_RENDERTARGETARRAYINDEX;
};


VS_OUT_CSM main(float4 position : POSITION, uint instanceId : SV_INSTANCEID)
{
    VS_OUT_CSM voutCSM;
    VS_OUT vout;
    
#if 1
    position.w = 1;
    vout.position = mul(position, mul(world, viewProjection));
    vout.wPosition = mul(position, world);

    voutCSM.instanceId = instanceId;
    voutCSM.position = mul(position, mul(world, csmData.cascadedMatrices[instanceId]));
#endif
    return voutCSM;
}