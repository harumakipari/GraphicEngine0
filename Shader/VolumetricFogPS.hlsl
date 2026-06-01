#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "Lights.hlsli"
#include "Sampler.hlsli"

Texture2D depthTexture : register(t0);
Texture2D positionMap : register(t1);
Texture2DArray cascadedShadowMaps : register(t2);
Texture3D noise3D : register(t31); // ノイズテクスチャ

// この霧ポイントが影の中か、光の中か判別する関数
float SunlightRadiance(float3 worldPositon)
{
    float4 posView = mul(float4(worldPositon, 1.0), view);
    float depthViewSpace = posView.z;

    //// ndc -> world space
    //float4 positionWorldSpace = mul(positionNdc, inverseViewProjection);
    //positionWorldSpace = positionWorldSpace / positionWorldSpace.w;
    
    // View 空間での深度を使って、どのカスケードに属するかを判定
    //float depthViewSpace = positionViewSpace.z;
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

    // カスケード外 → フォグの影響のみ
    if (cascadeIndex < 0)
    {
        return globalFogIntensity;
    }

    // フォグ内のワールド座標をライト空間（クリップ空間）へ変換
    //　world 空間 -> Light Clip 空間 (各カスケードに対応する Light View Projection 空間)
    float4 positionLightSpace = mul(float4(worldPositon.xyz, 1.0), cascadedMatrices[cascadeIndex]);
    positionLightSpace /= positionLightSpace.w; // Light Clip 空間 -> ndc 空間
    // ndc 空間 -> テクスチャ座標　
    positionLightSpace.x = positionLightSpace.x * +0.5 + 0.5;
    positionLightSpace.y = positionLightSpace.y * -0.5 + 0.5;
    
    // シャドウマップの深度と現在ピクセルの深度を比較
    // 1.0 -> 光が当たっている　0.0 -> 影の中
    return cascadedShadowMaps.SampleCmpLevelZero(comparisionSamplerState, float3(positionLightSpace.xy, cascadeIndex), positionLightSpace.z - shadowDepthBias).x + globalFogIntensity;
}

// 高さによる霧の濃度変化
void ApplyHeightFog(float3 position /*world space*/, inout float density)
{
    // 地面付近 → 濃い
    // 高空→ 薄い
    density *= exp(-(position.y - groundLevel) * fogHeightFalloff);
}

// 太陽方向に伸びる霧（前方散乱）
float MieScattering(float cosAngle /* 光の方向と視線の角度 */, float g /* 前方散乱の強さ*/)
{
    return (1.0 / (4.0 * 3.14159265358979)) * ((1 - (g * g)) / (pow((1 + (g * g)) - (2 * g) * cosAngle, 1.5)));
}

// カメラ → ピクセル位置まで霧の中をステップ移動して光を積分
float DitheredRayMarch(float2 screenPos, float3 rayStart, float3 rayDir, float rayLength, VS_OUT pin)
{
    float ditherValue = 0;
    if (enableDither)// ディザリング
    { // レイの開始位置を微妙にずらす
        const float4x4 ditherPattern =
        {
            { 0.0f, 0.5f, 0.125f, 0.625f },
            { 0.75f, 0.22f, 0.875f, 0.375f },
            { 0.1875f, 0.6875f, 0.0625f, 0.5625 },
            { 0.9375f, 0.4375f, 0.8125f, 0.3125 }
        };
        ditherValue = ditherPattern[screenPos.x % 4][screenPos.y % 4];
    }

    // レイ設定
    const int stepCount = 16;
    float stepSize = rayLength / stepCount;
    float3 step = rayDir * stepSize;
    
    float3 currentPosition = rayStart + step * ditherValue;
    
    float extinction = 0;
    float accumulatedRadiance = 0;
    [loop] // レイマーチループ
    for (int i = 0; i < stepCount; ++i)
    {
        // 太陽光のチェック
        float radiance = SunlightRadiance(currentPosition);
        // 霧密度
        float density = fogDensity;
#if 1
        // ノイズ（霧の揺らぎ）
        const float3 noiseVelocity = normalize(float3(1, 0, 0));
        float3 noiseSamplePosition = frac(currentPosition * noiseScale + noiseVelocity * elapsedTime * timeScale);
        float noise = 0.5 * noise3D.Sample(samplerStates[LINEAR], noiseSamplePosition) + 0.5;
        density *= noise;
#endif
        // 高さフォグ
        ApplyHeightFog(currentPosition, density);
        
        const float scatteringCoef = 0.815f;
        const float extinctionCoef = 0.0031f;
        float scattering = scatteringCoef * stepSize * density; // 散乱
        extinction += extinctionCoef * stepSize * density; // 減衰

        // 手前の霧ほど強く、奥は減衰 積分
        accumulatedRadiance += radiance * scattering * exp(-extinction);
        
        currentPosition += step;
    }
    
    const float cosAngle = dot(normalize(lightDirection.xyz), -rayDir);
    // Mie散乱補正
    accumulatedRadiance *= MieScattering(cosAngle, mieScatteringCoef);
    return accumulatedRadiance;
}




float main(VS_OUT pin) : SV_TARGET
{
    // 深度を取得
    float depth = depthTexture.Sample(samplerStates[POINT], pin.texcoord).x;
    // テクスチャ座標 -> world 空間
    float4 position = { 0, 0, 0, 0 };
    if (isWindowFog)
    { // こっちdepthTextureから復元する方法の方だと空にも霧がかかる。positionMapから復元する方法だと空には霧がかからない。
        position = mul(float4(pin.texcoord.x * 2.0 - 1.0, -pin.texcoord.y * 2.0 + 1.0, depth, 1.0), inverseViewProjection);
        position = position / position.w; // world 空間
    }
    else
    {
        position = positionMap.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord); // world 空間
    }
    // レイを生成
    float3 rayStart = cameraPositon.xyz;
    float3 rayDir = position.xyz - rayStart;
    float rayLength = length(rayDir);
    rayDir /= rayLength;
    
#if 1
    // 距離制限
    const float maxRayLength = fogCutoffDistance;
    rayLength = min(rayLength, maxRayLength);
#endif
    // レイマーチ実行
    return DitheredRayMarch(pin.position.xy, rayStart, rayDir, rayLength, pin);
}