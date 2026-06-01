#include "Sampler.hlsli"
#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"
#include "ModelType.hlsli"

Texture2D positionTexture : register(t0); // ワールド空間
Texture2D normalTexture : register(t1); // ワールド空間 w成分はSSRを使うかどうか 0:使わない 1:使う
Texture2D colorTexture : register(t2);
Texture2D materialTexture : register(t3); // x:metallic y:roughness z:occlusion w:occlusionStrength


cbuffer SSR_CONSTANTS_BUFFER : register(b5)
{
    float reflectionIntensity; // 反射の強さ
    float maxDistance; // 最大反射距離
    float resolution; // 探索の粗さ
    int steps; // 二分探索の回数
    float thickness; // ヒット判定の厚みの許容範囲
}

float2 NdcToUv(float2 ndc)
{
    float2 uv;
    uv.x = 0.5 + 0.5 * ndc.x;
    uv.y = 0.5 - ndc.y * 0.5;
    return uv;
}

// フレネル反射（FSchlick関数）
// 視線と法線の角度に基づいて、反射の強さを計算
inline float FSchlick(float f0, float cos)
{
    return f0 + (1 - f0) * pow(1 - cos, 5);
}

float3 main(VS_OUT pin) : SV_TARGET
{
    int steps = 10;

    uint2 dimensions;
    uint mipLevel = 0, numberOfLevels;
    positionTexture.GetDimensions(mipLevel, dimensions.x, dimensions.y, numberOfLevels);
    
    float4 position = positionTexture.Sample(samplerStates[LINEAR_BORDER_WHITE], pin.texcoord); // world空間
    float4 sampled = normalTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float3 normal = sampled.xyz; // world空間
    float4 materialValue = materialTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], pin.texcoord);
    float roughness = materialValue.y;

    int objectType = sampled.w; // 0:通常 1:プレイヤー 2:エネミー 3:stage

    if (objectType == OBJECT_PLAYER || objectType == OBJECT_ENEMY || objectType == OBJECT_NOT_SSR)
    { // プレイヤーとエネミーはSSRを使わない ステージのobjectでSSRをかけたくないものも追加
        return float3(0, 0, 0); // SSR なし
    }

    // world 空間　→　view 空間
    float4 positionView = mul(position, view);
    float3 normalView = normalize(mul(float4(normal, 0), view).xyz);

    float4 positionFrom = positionView;
    float4 positionTo = positionFrom;

    // 視線ベクトル
    float3 incident = normalize(positionFrom.xyz);
    // 反射ベクトル
    float3 reflection = normalize(reflect(incident, normalView.xyz));

    // view 空間でレイを定義する
    float4 startView = float4(positionFrom.xyz + (reflection * 0), 1);
    float4 endView = float4(positionFrom.xyz + (reflection * maxDistance), 1);

    if (endView.z < 0)
    { // 画面外に行かないように補正
        float3 v = endView.xyz - startView.xyz;
        endView.xyz = startView.xyz + v * abs(startView.z / v.z);
    }

    // view 空間 -> スクリーン空間
    float4 startFrag = mul(startView, projection); //　view 空間 -> クリップ空間
    startFrag /= startFrag.w; // クリップ空間 -> ndc 空間
    startFrag.xy = NdcToUv(startFrag.xy); // ndc 空間 -> テクスチャ空間
    startFrag.xy *= dimensions;
    
    // view 空間 -> スクリーン空間
    float4 endFrag = mul(endView, projection); //　view 空間 -> クリップ空間
    endFrag /= endFrag.w; // クリップ空間 -> ndc 空間
    endFrag.xy = NdcToUv(endFrag.xy); // ndc 空間 -> テクスチャ空間
    endFrag.xy *= dimensions;
    
    float2 frag = startFrag.xy;
    
    float4 uv = 0;
    uv.xy = frag / dimensions;

    // スクリーン空間のレイを設定する
    float deltaX = endFrag.x - startFrag.x;
    float deltaY = endFrag.y - startFrag.y;
    
    float useX = abs(deltaX) >= abs(deltaY) ? 1 : 0;
    float delta = lerp(abs(deltaY), abs(deltaX), useX) * clamp(resolution, 0, 1);

    // 1ステップで何ピクセル進むか
    float2 increment = float2(deltaX, deltaY) / max(delta, 0.001);
    
    float search0 = 0;
    float search1 = 0;
    
    int hit0 = 0;
    int hit1 = 0;
    
    float viewDistance = startView.z;
    float depth = thickness;
    
#define MAX_DELTA 64
    delta = min(MAX_DELTA, delta);
    [unroll(MAX_DELTA)] // レイマーチ
    for (int i = 0; i < (int) delta; ++i)
    {
        frag += increment;
        uv.xy = frag / dimensions;
        if (uv.x <= 0 || uv.x >= 1 || uv.y <= 0 || uv.y >= 1)
        {
            hit0 = 0;
            break;
        }

        // world 空間 -> view 空間
        positionTo = positionTexture.Sample(samplerStates[LINEAR_BORDER_WHITE], uv.xy); // ワールド空間
        positionTo = mul(float4(positionTo.xyz, 1.0), view);

        search1 = lerp((frag.y - startFrag.y) / deltaY, (frag.x - startFrag.x) / deltaX, useX);
        search1 = clamp(search1, 0.0, 1.0);

        // Perspective Correct Interpolation
        // NDC.z ベースで比較する
        // 深度比較
        viewDistance = (startView.z * endView.z) / lerp(endView.z, startView.z, search1);
        depth = viewDistance - positionTo.z;

        // ヒット判定
        if (depth > 0 && depth < thickness)
        { // レイが GBuffer の位置より奥で thickness 以内
            hit0 = 1;
            break;
        }
        else
        {
            search0 = search1;
        }
    }
#if 0
    hit1 = hit0;
#else
    search1 = search0 + ((search1 - search0) / 2.0);
    steps *= hit0;
    
    [unroll]    // 二分探索（ヒット精密化）
    for (i = 0; i < steps; ++i)
    {
        frag = lerp(startFrag.xy, endFrag.xy, search1);
        uv.xy = frag / dimensions;
        
        positionTo = positionTexture.Sample(samplerStates[LINEAR_BORDER_WHITE], uv.xy); // world 空間
        positionTo = mul(float4(positionTo.xyz, 1.0), view);

        // PerspectiveCorrect Interpolation
        viewDistance = (startView.z * endView.z) / lerp(endView.z, startView.z, search1);
        depth = viewDistance - positionTo.z;
        
        if (depth > 0 && depth < thickness)
        {
            hit1 = 1;
            search1 = search0 + ((search1 - search0) / 2);
        }
        else
        {
            float temp = search1;
            search1 = search1 + ((search1 - search0) / 2);
            search0 = temp;
        }
    }
#endif

    float visibility = hit1;
    visibility *= (1 - max(dot(-normalize(positionFrom.xyz), reflection), 0));
    visibility *= (1 - clamp(depth / thickness, 0, 1));
    visibility *= positionTo.w;
    visibility *= (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) ? 0 : 1;
    visibility = clamp(visibility, 0, 1);

#if 1
	// roughness減衰（単純エネルギー近似）
    visibility = lerp(visibility, 0, roughness * roughness);
#endif


    // フレネル＋色取得
    float fresnel = saturate(FSchlick(0.04, max(0, dot(reflection, normalView.xyz))));
    float3 reflectionColor = colorTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], uv.xy).rgb;
    reflectionColor = fresnel * reflectionColor * visibility * reflectionIntensity;

    return reflectionColor;
}