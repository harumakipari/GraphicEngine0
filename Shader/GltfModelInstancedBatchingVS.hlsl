#include "Constants.hlsli"
#include "GltfModel.hlsli"

INSTANCE_VS_OUT main(INSTANCE_VS_IN vsIn)
{
    INSTANCE_VS_OUT vsOut ;
    
    
    row_major float4x4 worldTransform = vsIn.instance_matrix;
    
    vsOut.position = mul(float4(vsIn.position.xyz, 1), mul(worldTransform, viewProjection));
    vsOut.wPosition = mul(float4(vsIn.position.xyz, 1), worldTransform);
    
    vsOut.wNormal = normalize(mul(float4(vsIn.normal.xyz, 0), worldTransform));
    vsOut.wNormal.w = 0;

    float sigma = vsIn.tangent.w;
    vsOut.wTangent = normalize(mul(float4(vsIn.tangent.xyz, 0), worldTransform));
    
    vsOut.wTangent.w = sigma;

    vsOut.instanceColor = vsIn.instanceColor;
    vsOut.instanceEmissive = vsIn.instanceEmissive;
    
    vsOut.texcoord = vsIn.texcoord;
    
    
    return vsOut;
}