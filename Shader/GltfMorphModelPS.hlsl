#include "GltfModel.hlsli"
#include "imageBasedLighting.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "Lights.hlsli"

#define BASECOLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);



// SHADOW
SamplerComparisonState comparisonSamplerState : register(s7);
//Texture2D shadowMap : register(t8)

float4 main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    //return float4(1, 1, 1, 1);
    const float GAMMA = 2.2;
    const MaterialConstants m = materials[material];
    
    float4 basecolorFactor = m.pbrMetallicRoughness.baseColorFactor;
    const int basecolorTexture = m.pbrMetallicRoughness.basecolorTexture.index;
    
    if (basecolorTexture > -1)
    {
        float4 sampled = materialTextures[BASECOLOR_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(saturate(sampled.rgb), GAMMA);
        basecolorFactor *= sampled;
    }
    
    if (m.alphaMode == 0 /*OPAQUE*/)
    {
        basecolorFactor.a = 1.0;
    }
    if (basecolorFactor.a < m.alphaCutoff)
    {
        discard;
    }
    
    float3 emissiveFactor = m.emissiveFactor;
    const int emissiveTexture = m.emissiveTexture.index;
    if (emissiveTexture > -1)
    {
        float4 sampled = materialTextures[EMISSIVE_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emissiveFactor *= sampled.rgb;
    }
    
    float roughnessFactor = m.pbrMetallicRoughness.roughnessFactor;
    float metallicFactor = m.pbrMetallicRoughness.metallicFactor;
    const int metallicRoughnessTexture = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
    if (metallicRoughnessTexture > -1)
    {
        float4 sampled = materialTextures[METALLIC_ROUGHNESS_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        roughnessFactor *= sampled.g;
        metallicFactor *= sampled.b;
    }
    
    float occlusionFactor = 1.0;
    const int occlusionTexture = m.occlusionTexture.index;
    if (occlusionTexture > -1)
    {
        float4 sampled = materialTextures[OCCLUSION_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        occlusionFactor *= sampled.r;
    }
    const float occlusionStrength = m.occlusionTexture.strength;
    
    const float3 f0 = lerp(0.04, basecolorFactor.rgb, metallicFactor);
    const float3 f90 = 1.0;
    const float alphaRoughness = roughnessFactor * roughnessFactor;
    const float3 cDiff = lerp(basecolorFactor.rgb, 0.0, metallicFactor);
    
    const float3 P = pin.wPosition.xyz;
    const float3 V = normalize(cameraPositon.xyz - pin.wPosition.xyz);
    
    float3 N = normalize(pin.wNormal.xyz);
    float3 T = hasTangent ? normalize(pin.wTangent.xyz) : float3(1, 0, 0);
    float sigma = hasTangent ? pin.wTangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    
    //For a back-facing surface, the tangential basis vectors are negated.
    if (isFrontFace == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
    
    const int normalTexture = m.normalTexture.index;
    if (normalTexture > -1)
    {
        float4 sampled = materialTextures[NORMAL_TEXTURE].Sample(samplerStates[LINEAR], pin.texcoord);
        float3 normalFactor = sampled.xyz;
        normalFactor = (normalFactor * 2.0) - 1.0;
        normalFactor = normalize(normalFactor * float3(m.normalTexture.scale, m.normalTexture.scale, 1.0));
        N = normalize((normalFactor.x * T) + (normalFactor.y * B) + (normalFactor.z * N));
    }
    //return float4(N * 0.5 + 0.5, 1);
    float3 diffuse = 0;
    float3 specular = 0;
  
#if 1  
     // Loop for shading process for each light 
    float3 L = normalize(-lightDirection.xyz);
    float3 Li = float3(colorLight.x, colorLight.y, colorLight.z) * colorLight.w; // Radiance of the light 
    float NoL = max(0, 0.5 * dot(N, L) + 0.5);
// 点光源の処理
    float3 pointDiffuse = 0;
    float3 pointSpecular = 0;
    if (pointLightEnable != 0)
    {
        for (int i = 0; i < pointLightCount; i++)
        {
            float3 LP = pin.wPosition.xyz - pointLights[i].position.xyz;
            float len = length(LP);
            if (len >= pointLights[i].range)
            {
                continue;
            }
            float attenuateLength = saturate(1.0 - len / pointLights[i].range);
            float attenuation = attenuateLength * attenuateLength;
            LP /= len;
            const float pNoV = max(0.0, dot(N, V));
            if (pNoV > 0.0 || pNoV > 0.0)
            {
                const float3 R = reflect(-LP, N);
                const float3 H = normalize(V + LP);
                float3 pLi = float3(pointLights[i].color.xyz) * pointLights[i].color.w; // Radiance of the light 
                const float NoH = max(0.0, dot(N, H));
                const float HoV = max(0.0, dot(H, V));
                float pNoL = max(0, 0.5 * dot(N, LP) + 0.5);
                pointDiffuse += pLi * pNoL * BrdfLambertian(f0, f90, cDiff, HoV);
                pointSpecular += pLi * pNoL * BrdfSpecularGgx(f0, f90, alphaRoughness, HoV, NoL, pNoV, NoH);
            }
        }
    }
    //テクスチャを貼る
    if (directionalLightEnable != 0)
    {
        const float NoV = max(0.0, dot(N, V));
        if (NoL > 0.0 || NoV > 0.0)
        {
            const float3 R = reflect(-L, N);
            const float3 H = normalize(V + L);
        
            const float NoH = max(0.0, dot(N, H));
            const float HoV = max(0.0, dot(H, V));
        
            diffuse += Li * NoL * BrdfLambertian(f0, f90, cDiff, HoV);
            specular += Li * NoL * BrdfSpecularGgx(f0, f90, alphaRoughness, HoV, NoL, NoV, NoH);
        }
    }
#endif
    
    //return baseColorFactor;

#if 1   //外の背景を移す
    //totalDiffuse += IblRadianceLambertian(N, V, roughnessFactor, cDiff, f0) * iblIntensity;
    float3 iblDiffuse = IblRadianceLambertian(N, V, roughnessFactor, cDiff, f0) * iblIntensity;
    //specular += IblRadianceGgx(N, V, roughnessFactor, f0);
    float3 iblSpecular = IblRadianceGgx(N, V, roughnessFactor, f0) * iblIntensity;
#endif
    
    float3 totalDiffuse = diffuse + pointDiffuse + iblDiffuse;
    float3 totalSpecular = specular + pointSpecular + iblSpecular;

    float3 emissive = emissiveFactor;
    diffuse = lerp(totalDiffuse, totalDiffuse * occlusionFactor, occlusionStrength);
    specular = lerp(totalSpecular, totalSpecular * occlusionFactor, occlusionStrength);



    float fresnel = pow(1.0 - saturate(dot(N, V)), 3.0);

    float3 baseColor = float3(0.2, 0.6, 0.9); // 水色
    float3 edgeColor = float3(0.8, 0.95, 1.0);
    // 中央は暗く、縁だけ明るい
    float3 color = lerp(baseColor, edgeColor, fresnel);

    // 少し暗くする

    return float4(color, 1.0);


    //float4 gbuffer3Color = float4(diffuse + specular + emmisive, basecolorFactor.a) * basecolorFactor;
    //float4 gbuffer3Color = float4(diffuse + specular + emmisive, basecolorFactor.a) * float4(finalColor.rgb, 1.0);
    //return gbuffer3Color;

    
    //return float4(Lo , baseColorFactor.a);
}