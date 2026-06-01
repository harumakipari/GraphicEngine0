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
Texture2DArray cascadedShadowMaps : register(t5);

cbuffer GAME_SCENE_CONSTANT_BUFFER : register(b12)
{
    float2 playerScreenPosition ; //プレイヤーの場所　死亡演出に必要な定数バッファ
    float2 screenSize; 
    float radius = 0.0f;

};



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

    bool isSky = (depthNdc == 0.0 || depthNdc >= 1.0);
    if (isSky)
    {
        return float4(color.rgb, 1.0);
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

    if (enableCascadedShadowMaps)
    {
        color.rgb = ApplyShadow(color.rgb, positionWorldSpace, (positionViewSpace.z), shadowMapDimensions, positionNdc.xyz, sceneNormal, lightDirection.xyz);
    }


    // ブルーム処理
    if (enableBloom)
    {
        float4 bloom = bloomTexture.Sample(samplerStates[POINT], pin.texcoord);
        color.rgb += bloom.rgb;
    }

    float4 finalColor = color;

    if (enableToneMapping == 1)
    {
        // トーンマップは共通にする
        //finalColor.rgb = JodieReinhardGameToneMap(finalColor.rgb);

	    // 色相、彩度、明度、コントラストを調整する。
        finalColor.rgb = HueSaturation(finalColor.rgb, hueShift, saturation);
        finalColor.rgb = BrightnessContrast(finalColor.rgb, brightness, contrast);

        // 色の調整をする
        finalColor.rgb = RGBColorMap(finalColor.rgb, colorMapRGB);
        finalColor.rgb = ToneCurve(finalColor.rgb, toneMappingValue);
    }


#if 0
    float2 uv = pin.texcoord;

    float2 delta = uv - playerScreenPosition;

    float aspectX =
    screenSize.x / screenSize.y;
    delta.x *= aspectX;

    float dist = length(delta);
    float softness = 0.08f;

    float mask =smoothstep(radius, radius + softness, dist);

    finalColor.rgb *= (1.0f - mask);
#endif

    //// リニア空間からsRGB空間
    const float GAMMA = 2.2;
    finalColor.rgb = pow(finalColor.rgb, 1.0 / GAMMA);


    return finalColor;
}