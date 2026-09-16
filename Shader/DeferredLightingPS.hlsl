#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "Lights.hlsli"
#include "Sampler.hlsli"
#include "ShaderFunctions.hlsli"
#include "ViewConstants.hlsli"
#include "ModelType.hlsli"

Texture2D normalMap : register(t0);
Texture2D materialMap : register(t1);
Texture2D colorMap : register(t2);
Texture2D positionMap : register(t3);
Texture2D emissiveMap : register(t4);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 sampled = normalMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float3 normal = sampled.xyz; // world space 
    int objectType = sampled.w;

    float4 baseColor = colorMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);

    float3 position = positionMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).xyz; // world space

    sampled = materialMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float metallicFactor = sampled.x;
    float roughnessFactor = sampled.y;
    float occlusionFactor = sampled.z;
    int materialType = sampled.w;
    
    sampled = emissiveMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float3 emissive = sampled.xyz;
    int gBufferFlag = sampled.w;

    if (objectType == OBJECT_NO_LIGHTING)
    { // ライティングがいらないものは
        return baseColor;
    }

    if (gBufferFlag == GBUFFER_FLAG_SKY)
    { // 何も書き込まれていなかったら スカイマップのために
        discard;
    }

    if (gBufferFlag == GBUFFER_FLAG_EMISSIVE)
    {
        return float4(emissive * 6.0, 1); // これsphereEmissiveに使用
    }

    // 目だけ追加
    if (materialType == MATERIAL_EYE)
    {
        float mask = step(0.8, baseColor.r);
        emissive += mask * float3(10, 0, 0);
    }

    const float3 f0 = lerp(0.04, baseColor.rgb, metallicFactor);
    const float3 f90 = 1.0;
    roughnessFactor = max(roughnessFactor, 0.05); // 最低値を作ることで、極端に鋭いスペキュラーを防止する


    const float alphaRoughness = roughnessFactor * roughnessFactor;
    const float3 cDiff = lerp(baseColor.rgb, 0.0, metallicFactor);

    const float3 N = normalize(normal);
    const float3 V = normalize(cameraPosition.xyz - position.xyz);
    
    // 光源の数をカウント
    int lightCount = 0;


    // 点光源の処理
    float3 pointDiffuse = 0;
    float3 pointSpecular = 0;
    if (pointLightEnable != 0)
    {
        for (int i = 0; i < pointLightCount; i++)
        {
            float3 LP = pointLights[i].position.xyz - position.xyz;
            float len = length(LP);

            float kc = attenuationPresets[pointLights[i].attenuationType].kc;
            float kl = attenuationPresets[pointLights[i].attenuationType].kl;
            float kq = attenuationPresets[pointLights[i].attenuationType].kq;

            float attenuation = saturate(1.0 / (kc + kl * len + kq * (len * len)));

            // ライト自体の強さ
            float3 pLi =pointLights[i].color.xyz * pointLights[i].color.w;

            // このライトが出せる最大寄与量を計算
            float maxLightIntensity =max(pLi.r, max(pLi.g, pLi.b));

            float contribution =maxLightIntensity * attenuation;

            // ほぼ見えないライトは、
            // normalize / GGX / Fresnelなどを行わない
            if (contribution < 0.001f)
            {
                continue;
            }

            LP /= len;
            const float pNoV = max(0.0, dot(N, V));
            const float pNoL = max(0.0, dot(N, LP));

             // 直接光なのでライトの反対側ならBRDF不要
            if (pNoL <= 0.0f)
            {
                continue;
            }


            if (pNoV > 0.0 || pNoL > 0.0) // 点光源には方向がないため
            {
                const float3 R = reflect(-LP, N);
                const float3 H = normalize(V + LP);

                float3 pLi = float3(pointLights[i].color.xyz) * pointLights[i].color.w; // 光の輝き

                const float NoH = max(0.0, dot(N, H));
                const float HoV = max(0.0, dot(H, V));

                pointDiffuse += pLi * pNoL * BrdfLambertian(f0, f90, cDiff, HoV) * attenuation ;
                pointSpecular += pLi * pNoL * BrdfSpecularGgx(f0, f90, alphaRoughness, HoV, pNoL, pNoV, NoH) * attenuation ;
            }
        }
        float maxPointSpecular = 3.0f;
        pointSpecular = max(0, min(maxPointSpecular, pointSpecular));
    }
#if 1
    // 平行光源の処理
    float3 diffuse = 0;
    float3 specular = 0;

    // 各光源に対するシェーディング処理のループ
    float3 L = normalize(-lightDirection.xyz);
    float3 Li = float3(colorLight.x, colorLight.y, colorLight.z) * colorLight.w; // 光の輝き 

    const float NoL = max(0, dot(N, L));
    //float NoL = saturate(dot(N, L) * 0.1 + 0.1);
    const float NoV = max(0.0, dot(N, V));

    if (directionalLightEnable != 0)
    {
        lightCount++;
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

    if (objectType == OBJECT_ENEMY)
    { // 敵の時は明るくする
        iblDiffuse = IblRadianceLambertian(N, V, roughnessFactor, cDiff, f0) * objectIblDiffuseIntensity;
        iblSpecular = IblRadianceGgx(N, V, roughnessFactor, f0) * objectIblSpecularIntensity;
    }

    if (objectType == OBJECT_DOOR)
    {
        iblSpecular *= 0.3;
    }
#endif

    // Keep each HDR contribution separate for the Deferred Lighting debug views.
    // These are the exact terms used by the normal composite below.
    const float3 directionalDiffuse = diffuse * occlusionFactor * diffuseIntensity;




    float3 directionalSpecular = specular * occlusionFactor * specularIntensity;
    const float3 pointLightDiffuse = pointDiffuse * pointLightDiffuseIntensity * occlusionFactor * diffuseIntensity;
    float3 pointLightSpecular = pointSpecular * pointLightSpecularIntensity * occlusionFactor * specularIntensity;
    const float3 iblDiffuseContribution = iblDiffuse * occlusionFactor * diffuseIntensity;
    float3 iblSpecularContribution = iblSpecular * occlusionFactor * specularIntensity;

    // These debug switches are deliberately restricted to enemy hair. They
    // zero only the selected specular contribution; diffuse and all other
    // materials remain untouched.
    const bool isHairEnemy = (objectType == OBJECT_ENEMY && materialType == MATERIAL_HAIR);
    //if (isHairEnemy)
    //{
    //    if ((hairSpecularDebugDisableMask & 1) != 0) directionalSpecular = 0.0f;
    //    if ((hairSpecularDebugDisableMask & 2) != 0) pointLightSpecular = 0.0f;
    //    if ((hairSpecularDebugDisableMask & 4) != 0) iblSpecularContribution = 0.0f;
    //}

    float3 totalDiffuse = directionalDiffuse + pointLightDiffuse + iblDiffuseContribution;
    float3 totalSpecular = directionalSpecular + pointLightSpecular + iblSpecularContribution;



#if 1
    float3 rim = 0;

    if (objectType == OBJECT_ENEMY && !isHairEnemy)
    {
        rim = CalcRimLight(N, V, rimColor, rimPower) * rimIntensity;
    }
    if (objectType == OBJECT_PLAYER)
    {
        rim = CalcRimLight(N, V, playerRimColor, rimPower) * playerRimIntensity;
        if (materialType == MATERIAL_HAIR)
        {
            rim = CalcRimLight(N, V, playerHairRimColor, rimPower) * playerHairRimIntensity;
        }
    }


#endif
    float3 ambient = baseColor.rgb * 0.05;

    const float3 directLight = directionalDiffuse + directionalSpecular;
    const float3 pointLight = pointLightDiffuse + pointLightSpecular;
    const float3 fullDeferredLighting = totalDiffuse + totalSpecular + emissive + rim /*+ ambient*/;

    // Deferred Lighting component debug views. FinalPS bypasses all post effects
    // for modes >= 2, so these values remain linear HDR (and intentionally may
    // clip on an SDR display when a channel exceeds 1.0).
    if (finalColorDebugMode == 4) return float4(totalDiffuse, 1.0f);                 // Diffuse Only
    if (finalColorDebugMode == 5) return float4(directLight, 1.0f);                  // Direct Light Only
    if (finalColorDebugMode == 6) return float4(pointLight, 1.0f);                   // Point Light Only
    if (finalColorDebugMode == 7) return float4(iblDiffuseContribution, 1.0f);      // IBL Diffuse Only
    if (finalColorDebugMode == 8) return float4(totalSpecular, 1.0f);                // Specular Only
    if (finalColorDebugMode == 9) return float4(rim, 1.0f);                          // Rim Only
    if (finalColorDebugMode == 10) return float4(emissive, 1.0f);                    // Emissive Only
    if (finalColorDebugMode == 11) return float4(directionalSpecular, 1.0f);         // Directional Specular Only
    if (finalColorDebugMode == 12) return float4(pointLightSpecular, 1.0f);          // Point Specular Only
    if (finalColorDebugMode == 13) return float4(iblSpecularContribution, 1.0f);     // IBL Specular Only

    return float4(fullDeferredLighting, 1.0f);
}