#include "GltfModel.hlsli"
#include "imageBasedLighting.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "Constants.hlsli"
#include "Lights.hlsli"

#define BASECOLOR_TEXTURE 0 
#define METALLIC_ROUGHNESS_TEXTURE 1 
#define NORMAL_TEXTURE 2 
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4 
Texture2D<float4> materialTextures[5] : register(t1);


#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
//SamplerState sampler_states[5] : register(s0);

Texture2D maskTexture : register(t11);

PS_OUT main(VS_OUT pin, bool isFrontFace : SV_IsFrontFace)
{
    PS_OUT pout;
    
    const float GAMMA = 2.2;
    const MaterialConstants m = materials[material];
    
    float4 basecolorFactor = m.pbrMetallicRoughness.baseColorFactor;
    const int basecolorTexture = m.pbrMetallicRoughness.basecolorTexture.index;
    
    if (basecolorTexture > -1)
    {
        float4 sampled = materialTextures[BASECOLOR_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
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
    
    float3 emmisiveFactor = m.emissiveFactor;
    const int emissiveTexture = m.emissiveTexture.index;
    if (emissiveTexture > -1)
    {
        float4 sampled = materialTextures[EMISSIVE_TEXTURE].Sample(samplerStates[ANISOTROPIC], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emmisiveFactor *= sampled.rgb;
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
    
    float3 diffuse = 0;
    float3 specular = 0;
  
#if 1   //テクスチャを貼る
     // Loop for shading process for each light 
    float3 L = normalize(-lightDirection.xyz);
    float3 Li = float3(colorLight.x, colorLight.y, colorLight.z) * colorLight.w; // Radiance of the light 
    //float3 Li = float3(1.0, 1.0, 1.0); // Radiance of the light 
    //const float NoL = max(0.0, dot(N, L));
    float NoL = max(0, 0.5 * dot(N, L) + 0.5);
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
#endif
    
    //return baseColorFactor;
    
#if 1   //外の背景を移す
    diffuse += IblRadianceLambertian(N, V, roughnessFactor, cDiff, f0) * iblIntensity;
    //specular += IblRadianceGgx(N, V, roughnessFactor, f0);
    specular += IblRadianceGgx(N, V, roughnessFactor, f0) * iblIntensity;
#endif
    
    
    float3 emmisive = emmisiveFactor;
    diffuse = lerp(diffuse, diffuse * occlusionFactor, occlusionStrength);
    specular = lerp(specular, specular * occlusionFactor, occlusionStrength);
    
    // MULTIPLE_RENDER_TARGETS
    pout.color = float4(diffuse + specular + emmisive, basecolorFactor.a) * basecolorFactor;
    
    //Dissolve
    float maskValue = maskTexture.Sample(samplerStates[ANISOTROPIC], pin.texcoord).x;

    float dissolveValue = 1.0; //　ここをアニメーションさせると良い

    float dissolve = saturate((dissolveValue - 0.5) * 2.0);
#if 1
    float alpha = step(dissolve, maskValue);
#else
    float edge = 0.02; // フェード幅
    float alpha = smoothstep(dissolve - edge, dissolve + edge, maskValue);
#endif
    pout.color.a *= alpha;
    
    if (abs(dissolve - maskValue) < 0.01f)
    {
        pout.color = float4(1, 0, 0, 1);
    }
    
    pout.position = mul(pin.wPosition, view); // to viewSpace
    pout.normal = mul(float4(N.xyz, 0), view); //to viewSpace;
    return pout;
    
    float3 Lo = diffuse + specular + emmisive;
    
    //return float4(Lo , baseColorFactor.a);
}