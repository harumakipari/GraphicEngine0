#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "Sampler.hlsli"
#include "FilterFunctions.hlsli"
#include "Lights.hlsli"
#include "ShaderFunctions.hlsli"
#include "ViewConstants.hlsli"

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
Texture2D velocityTexture : register(t10); // w にshadowFactorが入っている
Texture2DArray cascadedShadowMaps : register(t11);


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

float3 ApplyShadowMaps(inout float3 color, in float4 positionWorldSpace)
{
    float shadowFactor = 0.0;
    const float shadow_depth_bias = 0.001;
    float4 light_view_position = mul(positionWorldSpace, lightViewProjection); // World to Clip space
    light_view_position = light_view_position / light_view_position.w; // Clip to NDC
    float2 light_view_texcoord = 0;
	// NDC to Texture coordinate
    light_view_texcoord.x = light_view_position.x * +0.5 + 0.5;
    light_view_texcoord.y = light_view_position.y * -0.5 + 0.5;
    float depth = saturate(light_view_position.z - shadow_depth_bias);

    float3 shadow_factor = 1.0f;
    return shadow_factor;
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

    float NdotL = saturate(dot(normalize(normal), -normalize(lightDir)));
    float slope = 1.0 - NdotL;

// 調整パラメータ
    float baseBias = shadowDepthBias;

    float bias = baseBias + slope * slopeBias;
	// 硬い影
    shadowFactor = cascadedShadowMaps.SampleCmpLevelZero(comparisionSamplerState, float3(positionLightSpace.xy, cascadeIndex), positionLightSpace.z - bias).x;
	
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
                float sampledDepth = depthTexture.Sample(samplerStates[POINT], texcoord).x;
                distance = (depth - sampledDepth) * (depth - sampledDepth);
                const float sigma2 = 0.0001;
                float rangeGaussian = exp(-distance / sigma2); // ガウシアン関数で深度差に基づく重みを計算
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

    float fogStrength = fogFacter * fogColor.a;

    float transmittance = exp(-fogStrength);

    float3 finalColor =
    sceneColor * transmittance +
    fogColor.rgb * (1 - transmittance);

    return finalColor;
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
    // ndc -> world 
    float4 positionWorldSpace = mul(positionNdc, inverseViewProjection);
    positionWorldSpace /= positionWorldSpace.w;

    const float aspect = (float) height / width;

    if (enableCascadedShadowMaps && objectType != OBJECT_NO_LIGHTING)
    {
        color.rgb = ApplyShadow(color.rgb, positionWorldSpace, (positionViewSpace.z), shadowMapDimensions, positionNdc.xyz, sceneNormal, lightDirection.xyz);
        float shadowFactor = velocityTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord).w;
        float shadow = lerp(1.0, shadowFactor, shadowMapFactor);
        // 影色へ補間
        color.rgb = lerp(shadowMapColor, color.rgb, shadow);
    }


    if (enableFog && objectType != OBJECT_NO_LIGHTING)
    {
        float linearDepth = positionViewSpace.z;

        // Volumetric Fog
        color.rgb = CalculatedFogColor(pin.texcoord, depthNdc, color.rgb);
    }

    // ブルーム処理
    if (enableBloom && objectType != OBJECT_NO_LIGHTING)
    {
        float4 bloom = bloomTexture.Sample(samplerStates[POINT], pin.texcoord);
        color.rgb += bloom.rgb;
    }

    // SSRの処理
    if (enableSSR && objectType != OBJECT_NO_LIGHTING)
    {
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
        if (objectType != OBJECT_PLAYER && objectType != OBJECT_NO_LIGHTING)
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
}