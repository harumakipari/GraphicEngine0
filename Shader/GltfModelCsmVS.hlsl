// CASCADED_SHADOW_MAPS
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


VS_OUT_CSM main(VS_IN vin, uint instanceId : SV_INSTANCEID)
{
    VS_OUT_CSM vout;
    
#if 0
    float4 blendedPosition = { 0, 0, 0, 1 };
    for (int boneIndex = 0; boneIndex < 4;++boneIndex)
    {
        blendedPosition+=vin.weights[boneIndex]
    
            uint jointIndex = (i < 4) ? vin.joints[0][i] : vin.joints[1][i - 4];
        float weight = (i < 4) ? vin.weights[0][i] : vin.weights[1][i - 4];

        blendedPosition += weight * mul(vin.position, jointMatrices[jointIndex]);
    }
#else
    if (skin > -1)
    {
        row_major float4x4 skinMatrix =
        jointMatrices[vin.joints[0].x] * vin.weights[0].x +
        jointMatrices[vin.joints[0].y] * vin.weights[0].y +
        jointMatrices[vin.joints[0].z] * vin.weights[0].z +
        jointMatrices[vin.joints[0].w] * vin.weights[0].w +
        jointMatrices[vin.joints[1].x] * vin.weights[1].x +
        jointMatrices[vin.joints[1].y] * vin.weights[1].y +
        jointMatrices[vin.joints[1].z] * vin.weights[1].z +
        jointMatrices[vin.joints[1].w] * vin.weights[1].w;
        vin.position = mul(float4(vin.position.xyz, 1), skinMatrix);
    }

    vout.instanceId = instanceId;
    vout.position = mul(vin.position, mul(world, csmData.cascadedMatrices[instanceId]));
#endif
    return vout;
}