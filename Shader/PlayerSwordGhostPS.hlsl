// プレイヤーの剣の残像処理

#include "GltfModel.hlsli"
#include "imageBasedLighting.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "Lights.hlsli"
#include "ShaderFunctions.hlsli"

#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);

float4 main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    float3 N = normalize(pin.wNormal.xyz);
    float3 V = normalize(cameraPositon.xyz - pin.wPosition.xyz);

    float fresnel = pow(1.0 - saturate(dot(N, V)), 5.0);

    float3 finalColor = cpuColor.rgb * emissionPower;

    finalColor += float3(1, 1, 1) * fresnel;

    return float4(finalColor, cpuColor.a);


}