#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "imageBasedLighting.hlsli"
#include "BidirectionalReflectanceDistributionFunction.hlsli"
#include "Lights.hlsli"
#include "Sampler.hlsli"
#include "ShaderFunctions.hlsli"

Texture2D normalMap : register(t0);
Texture2D materialMap : register(t1);
Texture2D colorMap : register(t2);
Texture2D positionMap : register(t3);
Texture2D emissiveMap : register(t4);

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 normal = normalMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).xyz; // world space 

    float4 baseColor = colorMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);

    float3 position = positionMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).xyz; // world space

    float4 sampled = materialMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float metallicFactor = sampled.x;
    float roughnessFactor = sampled.y;
    float occlusionFactor = sampled.z;
    int materialType = sampled.w;
    
    sampled = emissiveMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float3 emissive = sampled.xyz;
    float skymap = sampled.w;
    float emissiveFlag = skymap;

    if (skymap == 1)
    { // 何も書き込まれていなかったら スカイマップのために
        discard;
    }

    const float3 f0 = lerp(0.04, baseColor.rgb, metallicFactor);
    const float3 f90 = 1.0;
    roughnessFactor = max(roughnessFactor, 0.3); // 最低値を作ることで、極端に鋭いスペキュラーを防止する
    const float alphaRoughness = roughnessFactor * roughnessFactor;
    const float3 cDiff = lerp(baseColor.rgb, 0.0, metallicFactor);

    const float3 N = normalize(normal);
    const float3 V = normalize(cameraPositon.xyz - position.xyz);
    

    // 点光源の処理
    float3 pointDiffuse = 0;
    float3 pointSpecular = 0;
    if (pointLightEnable != 0)
    {
        for (int i = 0; i < pointLightCount; i++)
        {
            float3 LP = pointLights[i].position.xyz - position.xyz;
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

            float attenuation = saturate(1.0 / (kc + kl * len + kq * (len * len)));
#else
            //float attenuation = attenuateLength * attenuateLength;

            float distanceAtt = 1.0 / (1.0 + len * len);
            float rangeAtt = saturate(1.0 - len / pointLights[i].range);
            rangeAtt *= rangeAtt;

            float attenuation = distanceAtt * rangeAtt;

#endif
            LP /= len;
            const float pNoV = max(0.0, dot(N, V));
            const float pNoL = max(0.0, dot(N, LP));

            if (pNoV > 0.0 || pNoL > 0.0) // 点光源には方向がないため
            {
                const float3 R = reflect(-LP, N);
                const float3 H = normalize(V + LP);

                float3 pLi = float3(pointLights[i].color.xyz) * pointLights[i].color.w; // 光の輝き

                const float NoH = max(0.0, dot(N, H));
                const float HoV = max(0.0, dot(H, V));

                float attenuationRate = lightDirection.w;
                pointDiffuse += pLi * pNoL * BrdfLambertian(f0, f90, cDiff, HoV) * lerp(1.0, attenuation, attenuationRate);
                pointSpecular += pLi * pNoL * BrdfSpecularGgx(f0, f90, alphaRoughness, HoV, pNoL, pNoV, NoH) * lerp(1.0, attenuation, attenuationRate);
            }
        }
    }
#if 1
    // 平行光源の処理
    float3 diffuse = 0;
    float3 specular = 0;

    // 各光源に対するシェーディング処理のループ
    float3 L = normalize(-lightDirection.xyz);
    float3 Li = float3(colorLight.x, colorLight.y, colorLight.z) * colorLight.w; // 光の輝き 


    //const float NoL = max(0, dot(N, L));
    float NoL = saturate(dot(N, L) * 0.5 + 0.5);
    const float NoV = max(0.0, dot(N, V));

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

    float3 totalDiffuse = diffuse + (pointDiffuse * pointLightDiffuseIntensity) + iblDiffuse;
    float3 totalSpecular = specular + (pointSpecular * pointLightSpecularIntensity) + iblSpecular;


    totalDiffuse = totalDiffuse * occlusionFactor * diffuseIntensity;
    totalSpecular = totalSpecular * occlusionFactor * specularIntensity;

#if 0
    float3 rim = CalcRimLight(N, V, rimColor.rgb, 3.0) * rimIntensity;
    if (baseColor.a < 1.0)
        rim = 0;
#endif
    float3 ambient = baseColor.rgb * 0.05;

    float3 lo = totalDiffuse + totalSpecular + emissive /*+ rim*/ + ambient;

    return float4(lo, 1.0f);
}