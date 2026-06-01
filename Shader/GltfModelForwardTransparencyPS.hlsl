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
    
    const float3 f0 = lerp(0.04, baseColorFactor.rgb, metallicFactor);
    const float3 f90 = 1.0;
    const float alphaRoughness = roughnessFactor * roughnessFactor;
    const float3 cDiff = lerp(baseColorFactor.rgb, 0.0, metallicFactor);
    
    const float3 V = normalize(cameraPositon.xyz - pin.wPosition.xyz);
    
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
    if (pointLightEnable != 0)
    {
        for (int i = 0; i < pointLightCount; i++)
        {
            float3 LP = pointLights[i].position.xyz - pin.wPosition.xyz;
            //float3 LP = position.xyz - pointLights[i].position.xyz; // world space 点光源の方向
            float len = length(LP);
            //if (len >= pointLights[i].range)
            //{
            //    continue;
            //}

            float attenuateLength = saturate(1.0 - len / pointLights[i].range);
#if 1
            /*	
            		Distance	Kc		Kl		Kq
            		7			1		0.7		1.8
            		13			1		0.35	0.44
            		20			1		0.22	0.2
            		32			1		0.14	0.07
            		50			1		0.09	0.032
            		65			1		0.07	0.017
            		100			1		0.045	0.0075
            		160			1		0.027	0.0028
            		200			1		0.022	0.0019
            		325			1		0.014	0.0007
            		600			1		0.007	0.0002
            		3250		1		0.0014	0.000007	
            */

            //float Kc = 1.0; // attenuation_constant
            //float Kl = 0.35; // attenuation_linear
            //float Kq = 0.44; // attenuation_quadratic

            float Kc = 1.0; // attenuation_constant
            float Kl = 0.7; // attenuation_linear
            float Kq = 1.8; // attenuation_quadratic
            float attenuation = saturate(1.0 / (Kc + Kl * len + Kq * (len * len)));
#else
            float attenuation = attenuateLength * attenuateLength;

            //float distanceAtt = 1.0 / (1.0 + len * len);
            //float rangeAtt = saturate(1.0 - len / pointLights[i].range);
            //rangeAtt *= rangeAtt;

            //float attenuation = distanceAtt * rangeAtt;

#endif
            LP /= len;
            const float pNoV = max(0.0, dot(N, V));
            const float pNoL = max(0.0, dot(N, LP));
            //float pNoL = max(0, 0.8 * dot(N, LP) + 0.8);

            if (pNoV > 0.0 || pNoL > 0.0) // 点光源には方向がないため
            {
                const float3 R = reflect(-LP, N);
                const float3 H = normalize(V + LP);

                float3 pLi = float3(pointLights[i].color.xyz) * pointLights[i].color.w; // 光の輝き

                const float NoH = max(0.0, dot(N, H));
                const float HoV = max(0.0, dot(H, V));

                pointDiffuse += pLi * pNoL * BrdfLambertian(f0, f90, cDiff, HoV) * lerp(1.0, attenuation, lightDirection.w);
                //pointDiffuse += pLi * pNoL * BrdfLambertian(f0, f90, cDiff, HoV) * attenuation;
                pointSpecular += pLi * pNoL * BrdfSpecularGgx(f0, f90, alphaRoughness, HoV, pNoL, pNoV, NoH) * attenuation;
            }
        }
    }
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
#if 0 
    float rimPower = lightDirection.w;
    float3 rim = CalcRimLight(N, V, rimColor.rgb, rimPower) * rimIntensity;
#endif
    float3 Lo = totalDiffuse + totalSpecular + emissive /*+ rim*/;
	
    return float4(Lo, baseColorFactor.a);

    float4 color = float4(diffuse + specular + emissiveFactor, baseColorFactor.a) * baseColorFactor;
    return color;

    
    //return float4(Lo , baseColorFactor.a);
}