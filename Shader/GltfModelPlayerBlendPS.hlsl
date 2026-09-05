#include "GltfModel.hlsli"
#include "imageBasedLighting.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "FilterFunctions.hlsli"
#include "Lights.hlsli"
#include "ShaderFunctions.hlsli"

#define BASE_COLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);

float Bayer4x4(float2 screenPosition)
{
    static const float bayer[16] =
    {
         0.0f,  8.0f,  2.0f, 10.0f,
        12.0f,  4.0f, 14.0f,  6.0f,
         3.0f, 11.0f,  1.0f,  9.0f,
        15.0f,  7.0f, 13.0f,  5.0f
    };

    const int2 pixel = int2(floor(screenPosition));
    const int x = pixel.x & 3;
    const int y = pixel.y & 3;
    return (bayer[y * 4 + x] + 0.5f) / 16.0f;
}

float4 main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace) : SV_TARGET0
{
    const float GAMMA = 2.2;
    const MaterialConstants m = materials[material];
    
    float4 baseColorFactor = m.pbrMetallicRoughness.baseColorFactor;
    const int baseColorTexture = m.pbrMetallicRoughness.basecolorTexture.index;
    
    if (baseColorTexture > -1)
    {
        float4 sampled = materialTextures[BASE_COLOR_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(saturate(sampled.rgb), GAMMA);
        baseColorFactor *= sampled;
    }

    float3 emissiveFactor = m.emissiveFactor;
    const int emissiveTexture = m.emissiveTexture.index;
    if (emissiveTexture > -1)
    {
        float4 sampled = materialTextures[EMISSIVE_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emissiveFactor *= sampled.rgb;
        emissiveFactor *= emissionPower;
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

    const float damageFlashAmount = saturate(flashValue);
    const float damageBodyAmount = saturate(
        damageFlashAmount * modelEffectParameter.edgeWidth);
    baseColorFactor.rgb = lerp(
        baseColorFactor.rgb, cpuColor.rgb, damageBodyAmount);
    if (materialType == MATERIAL_CLOTH || materialType == MATERIAL_FUR)
    {
        baseColorFactor.rgb = HueSaturation(baseColorFactor.rgb, modelHueShift, modelSaturation);
        baseColorFactor.rgb = BrightnessContrast(baseColorFactor.rgb, modelBrightness, modelContrast);
    }
    
    const float3 f0 = lerp(0.04, baseColorFactor.rgb, metallicFactor);
    const float3 f90 = 1.0;
    const float alphaRoughness = roughnessFactor * roughnessFactor;
    const float3 cDiff = lerp(baseColorFactor.rgb, 0.0, metallicFactor);
    
    const float3 V = normalize(cameraPosition.xyz - pin.wPosition.xyz);
    
    float3 N = normalize(pin.wNormal.xyz);
    float3 T = hasTangent ? normalize(pin.wTangent.xyz) : float3(1, 0, 0.0001);
    float sigma = hasTangent ? pin.wTangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
    
    //背面については、接線方向の基底ベクトルは符号が反転する。
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

  
#if 1
    // 点光源の処理
    float3 pointDiffuse = 0;
    float3 pointSpecular = 0;

    // 平行光源の処理
    float3 diffuse = 0;
    float3 specular = 0;

     // 各光源に対するシェーディング処理のループ 
    float3 L = normalize(-lightDirection.xyz);
    float3 Li = float3(colorLight.x, colorLight.y, colorLight.z) * colorLight.w; //  光の輝き

    float NoL = saturate(dot(N, L) * 0.5 + 0.5);
    const float NoV = max(0, dot(N, V));

    if (directionalLightEnable != 0)
    {
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
    

#if 1   // 画像ベースの照明
    float3 iblDiffuse = IblRadianceLambertian(N, V, roughnessFactor, cDiff, f0) * iblIntensity;
    float3 iblSpecular = IblRadianceGgx(N, V, roughnessFactor, f0) * iblIntensity;
#endif
    float3 totalDiffuse = diffuse + pointDiffuse + iblDiffuse;
    float3 totalSpecular = specular + pointSpecular + iblSpecular;

    totalDiffuse = lerp(totalDiffuse, totalDiffuse * occlusionFactor, occlusionStrength);
    totalSpecular = lerp(totalSpecular, totalSpecular * occlusionFactor, occlusionStrength);

    float3 emissive = emissiveFactor;
    if (materialType == MATERIAL_EYE)
    {
        const float luminance = dot(baseColorFactor.rgb, float3(0.3, 0.59, 0.11));
        const float dist = distance(pin.texcoord, float2(0.5, 0.5));
        const float mask = (1.0 - step(0.1, luminance)) * (1.0 - smoothstep(0.1, 0.2, dist));
        emissive = mask * cpuColor.rgb * emissionPower;
    }
    const float damageRimFactor = pow(
        1.0f - saturate(dot(N, V)), 3.0f);
    emissive += cpuColor.rgb
        * damageRimFactor
        * damageFlashAmount
        * modelEffectParameter.edgePower;
#if 1
    float rimPower = lightDirection.w;
    float3 rimColorForPlayer = playerRimColor;
    float rimIntensityForPlayer = playerRimIntensity;
    if (materialType == MATERIAL_HAIR)
    {
        rimColorForPlayer = playerHairRimColor;
        rimIntensityForPlayer = playerHairRimIntensity;
    }
    float3 rim = CalcRimLight(N, V, rimColorForPlayer, rimPower) * rimIntensityForPlayer;
#endif
    float3 Lo = totalDiffuse + totalSpecular + emissive + rim;

    const float alpha = saturate(cpuColor.a);
    const float threshold = Bayer4x4(pin.position.xy);
    clip(alpha - threshold);

    return float4(Lo, 1.0f);

}
