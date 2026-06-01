#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "Sampler.hlsli"
#include "FilterFunctions.hlsli"
#include "Lights.hlsli"
#include "ModelType.hlsli"
#include "ShaderFunctions.hlsli"

Texture2D colorTexture : register(t0);
Texture2D positionTexture : register(t1);
Texture2D normalTexture : register(t2);
Texture2D depthTexture : register(t3);
Texture2D bloomTexture : register(t4);
Texture2D fogTexture : register(t5);
Texture2D ssaoTexture : register(t6);
Texture2D ssrTexture : register(t7);
Texture2D bokehTexture : register(t8);
Texture2D sceneColorTexture : register(t9);
Texture2DArray cascadedShadowMaps : register(t10);

Texture3D noise3D : register(t20); // ノイズテクスチャ


// texcoord -> ndc 空間に変換
float4 CalculatedPositionNDC(VS_OUT pin)
{
    float depthNdc = depthTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).x;
    float4 positionNdc;
    positionNdc.x = pin.texcoord.x * +2 - 1;
    positionNdc.y = pin.texcoord.y * -2 + 1;
    positionNdc.z = depthNdc;
    positionNdc.w = 1;
    return positionNdc;
}



float3 ApplyShadow(inout float3 color, in float4 positionWorldSpace, in float depthViewSpace, in float2 shadowMapDimensions, in float3 randSeed, in float3 normal, in float3 lightDir)
{
    float shadowFactor = 0.0;
	
    // カスケードされたビューフラスタムボリュームの層を見つける
    int cascadeIndex = -1;
    for (uint layer = 0; layer < 4; ++layer)
    {
        float distance = cascadedPlaneDistances[layer];
        if (distance > depthViewSpace)
        {
            cascadeIndex = layer;
            break;
        }
    }
	// 外側の遠方パネル
    if (cascadeIndex == -1)
    {
        return color;
    }
	
	// ワールド空間からライトビュークリップ空間へ、そしてNDCへ
    float4 positionLightSpace = mul(positionWorldSpace, cascadedMatrices[cascadeIndex]);
    positionLightSpace /= positionLightSpace.w;
	// ndc からテクスチャ空間へ
    positionLightSpace.x = positionLightSpace.x * +0.5 + 0.5;
    positionLightSpace.y = positionLightSpace.y * -0.5 + 0.5;

#if 1
    float NdotL = saturate(dot(normalize(normal), -normalize(lightDir)));
    float slope = 1.0 - NdotL;

// 調整パラメータ
    float baseBias = shadowDepthBias;

    float bias = baseBias + slope * slopeBias;
	// 硬い影
    shadowFactor = cascadedShadowMaps.SampleCmpLevelZero(comparisionSamplerState, float3(positionLightSpace.xy, cascadeIndex), positionLightSpace.z - bias).x;
#else
	// ソフトシャドウ
    const float2 sampleScale = (0.5 * shadow_filter_radius) / shadowMapDimensions;
    float accum = 0.0;
    for (uint sample_index = 0; sample_index < shadow_sample_count; ++sample_index)
    {
        float2 sampleOffset;
        float4 seed = float4(randSeed, sample_index);
        uint random = (uint) (64.0 * frac(sin(dot(seed, float4(12.9898, 78.233, 45.164, 94.673))) * 43758.5453)) % 64;
        sampleOffset = poisson_samples[random] * sampleScale;

        float2 sample_position = positionLightSpace.xy + sampleOffset;
        accum += cascadedShadowMaps.SampleCmpLevelZero(comparisionSamplerState, float3(sample_position, cascadeIndex), positionLightSpace.z - shadowDepthBias).x;
    }
    shadowFactor = accum / shadow_sample_count;
#endif
	
#if 1
    if (colorizeCascadedLayer)
    {
        const float3 colors[4] =
        {
            { 1, 0, 0 },
            { 0, 1, 0 },
            { 0, 0, 1 },
            { 1, 1, 0 },
        };
        return color * lerp(shadowColor, 1.0, shadowFactor) * colors[cascadeIndex];
    }
#endif
	
    return color * lerp(shadowColor, 1.0, shadowFactor);
}


float3 CalculatedFogColor(float2 uv, float depth, float3 sceneColor)
{
    uint2 depthMapDimensions;
    uint depthMipLevel = 0, numberOfSamples, levels;
    fogTexture.GetDimensions(depthMipLevel, depthMapDimensions.x, depthMapDimensions.y, numberOfSamples);
    
    float fogFacter = 0;
    if (enableBlur)
    {
        float accumulatedRadiance = 0.0;
        float accumulatedWeight = 0.0;
        const float radius = 4.0;
        for (float x = -radius; x <= radius; x += 1.0)
        {
            for (float y = -radius; y <= radius; y += 1.0)
            {
                float2 offset = float2(x, y) / depthMapDimensions;
                float2 texcoord = uv + offset;
                
                float sampledRadiance = fogTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord).x;

                float distance = x * x + y * y;
                const float sigma = 2.0 * radius * radius;
                float domainGaussian = exp(-distance / sigma); // ガウシアン関数で距離に基づく重みを計算

#if 1
                float sampledDepth = depthTexture.Sample(samplerStates[POINT], texcoord).x;
                distance = (depth - sampledDepth) * (depth - sampledDepth);
                const float sigma2 = 0.0001;
                float rangeGaussian = exp(-distance / sigma2); // ガウシアン関数で深度差に基づく重みを計算
#else
                float sampledDepthNdc = depthTexture.Sample(samplerStates[POINT], texcoord).x;
                // NDC → View に変換
                float4 ndc = float4(texcoord.x * 2 - 1, texcoord.y * -2 + 1, sampledDepthNdc, 1);
                float4 view = mul(ndc, inverseProjection);
                view /= view.w;
                const float sigma2 = 0.0001;
                float sampledLinearDepth = view.z;
                float diff = depth - sampledLinearDepth;
                float rangeGaussian = exp(-(diff * diff) / sigma2);
#endif
                accumulatedRadiance += sampledRadiance * domainGaussian * rangeGaussian;
                accumulatedWeight += domainGaussian * rangeGaussian;
            }
        }
        fogFacter = accumulatedRadiance / max(accumulatedWeight, 0.00001f);
    }
    else
    {
        fogFacter = fogTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], uv).x;
    }

#if 1
    float fogStrength = fogFacter * fogColor.a;

    float transmittance = exp(-fogStrength);

    float3 finalColor =
    sceneColor * transmittance +
    fogColor.rgb * (1 - transmittance);

    return finalColor;
#else
    float3 finalFogColor = fogColor.rgb * fogColor.a * max(0, fogFacter);
    return finalFogColor;
#endif
}

float4 main(VS_OUT pin) : SV_TARGET
{
    uint mipLevel = 0, width, height, numberOfLevel, levels;
    colorTexture.GetDimensions(mipLevel, width, height, numberOfLevel);

    uint2 shadowMapDimensions;
    cascadedShadowMaps.GetDimensions(mipLevel, shadowMapDimensions.x, shadowMapDimensions.y, numberOfLevel, levels);

    // シーンからライティング済みのカラーテクスチャ
    float4 sceneColor = colorTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float4 color = sceneColor;

    // シーンから法線を取得
    float4 sceneNormal = normalTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    int objectType = sceneNormal.w;

    // シーンから深度値を取得
    float depthNdc = depthTexture.Sample(samplerStates[POINT], pin.texcoord).x;

    if (renderStep == 1)
    {
        float4 baseColor = sceneColorTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
        return baseColor;
    }



    float4 positionNdc;
    // uv -> ndc 
    positionNdc.x = pin.texcoord.x * +2 - 1;
    positionNdc.y = pin.texcoord.y * -2 + 1;
    positionNdc.z = depthNdc;
    positionNdc.w = 1;

    // ndc -> view 
    float4 positionViewSpace = mul(positionNdc, inverseProjection); // ndc → clip 
    positionViewSpace = positionViewSpace / positionViewSpace.w; // clip -> view 
    //return float4((positionViewSpace.z) / 100.0, 0, 0, 1);
    // ndc -> world 
    float4 positionWorldSpace = mul(positionNdc, inverseViewProjection);
    positionWorldSpace /= positionWorldSpace.w;

    const float aspect = (float) height / width;

    if (enableCascadedShadowMaps)
    {
        color.rgb = ApplyShadow(color.rgb, positionWorldSpace, (positionViewSpace.z), shadowMapDimensions, positionNdc.xyz, sceneNormal, lightDirection.xyz);
    }


    if (enableFog)
    {
        float linearDepth = positionViewSpace.z;

        // Volumetric Fog
        color.rgb = CalculatedFogColor(pin.texcoord, depthNdc, color.rgb);
#if 0
        // 距離fog
        color = CalcFog(color, fogColor, float2(fogNear, fogFar), length(positionWorldSpace.xyz - cameraPositon.xyz));


        float dist = length(positionWorldSpace.xyz - cameraPositon.xyz);
        // 距離
        float distFog = saturate((dist - fogNear) / (fogFar - fogNear));
        // 高さ
        float height = positionWorldSpace.y;
        float heightFog = exp(-height * distanceFogHeightFalloff);
        // ノイズ
        const float3 noiseVelocity = normalize(float3(1, 0, 0));
        float3 noiseSamplePosition = frac(positionWorldSpace * noiseScale + noiseVelocity * elapsedTime * timeScale);
        float noise = 0.5 * noise3D.Sample(samplerStates[LINEAR], noiseSamplePosition) + 0.5;
        heightFog *= lerp(0.8, 1.2, noise);

        float fogFactor = 1 - exp(-distFog * heightFog);
        color.rgb = lerp(color.rgb, fogColor.rgb, fogFactor);
#endif
    }

    // ブルーム処理
    if (enableBloom)
    {
        float4 bloom = bloomTexture.Sample(samplerStates[POINT], pin.texcoord);
        color.rgb += bloom.rgb;
    }

    // SSRの処理
    if (enableSSR)
    {
        //float3 reflectColor = reflectionTexture.Sample(samplerStates[LINEAR_CLAMP], pin.texcoord).rgb;
        //return float4(reflectColor.rgb, 1);

        color.rgb += ssrTexture.Sample(samplerStates[LINEAR_CLAMP], pin.texcoord).rgb;
    }

    // SSAOの処理
    if (enableSSAO)
    {
        const float radius = 4.0;
        const float sigma = 2.0 * radius * radius;
        const float sigma2 = 0.01; // 深度の差に対してどれくらい敏感に反応するかを決定するパラメータ
        // この値が小さいほど、わずかな段差でも「エッジ」と見なしてぼかさなくなる
        float currDepth = depthNdc;
        float weight = 0.0;
	
        float accumulatedOcclusion = 0;
	
        for (float i = -radius; i <= radius; i += 1.0)
        {
            for (float j = -radius; j <= radius; j += 1.0)
            {
                float dx = i / width;
                float dy = j / height;
                float2 uv = float2(pin.texcoord.x + dx, pin.texcoord.y + dy);
			
                float distance = i * i + j * j;
                float domainGaussian = exp(-distance / sigma); // ガウシアン関数で距離に基づく重みを計算
			
                float sampleDepth = depthTexture.SampleLevel(samplerStates[POINT], uv, 0).x;
                distance = (currDepth - sampleDepth) * (currDepth - sampleDepth);
                float rangeGaussian = exp(-distance / sigma2); // ガウシアン関数で深度差に基づく重みを計算
			
			    //  サンプル遮蔽（環境）係数
                float sampleOcclusion = ssaoTexture.SampleLevel(samplerStates[LINEAR_BORDER_BLACK], uv, 0).x;
                accumulatedOcclusion += sampleOcclusion * domainGaussian * rangeGaussian;

                weight += domainGaussian * rangeGaussian;
            }
        }
        float occlusion = accumulatedOcclusion / weight;
        if (objectType != OBJECT_PLAYER)
        { // プレイヤーはSSAOの影響を受けないようにする
            color *= occlusion;
        }
    }

    float4 finalColor = color;


    // 分割表示
    if (pin.texcoord.x < splitU)
    {
        // 左側はポストなし
        finalColor = sceneColor;
    }

    return finalColor;
    //// DOFの処理
    //if (enableDof)
    //{
    //    // 深度からview space Z
    //    float viewSpaceZ = positionViewSpace.z;

    //    // ブレンド係数
    //    float alpha = abs(viewSpaceZ - focusDistance) / dofRange;
    //    alpha = saturate(alpha);

    //    // 色取得
    //    float3 originColor = color.rgb;
    //    float3 bokehColor = bokehTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).rgb;

    //    // DOF合成
    //    color.rgb = lerp(originColor, bokehColor, alpha);
    //}



    //// 分割表示
    //if (pin.texcoord.x < splitU)
    //{
    //    // 左側はポストなし
    //    finalColor = sceneColor;
    //}

    if (enableToneMapping == 1)
    {
        // トーンマップは共通にする
        finalColor.rgb = JodieReinhardToneMap(finalColor.rgb);

	    // 色相、彩度、明度、コントラストを調整する。
        finalColor.rgb = HueSaturation(finalColor.rgb, hueShift, saturation);
        finalColor.rgb = BrightnessContrast(finalColor.rgb, brightness, contrast);
    }

    return finalColor;
}