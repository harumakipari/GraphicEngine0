#include "Constants.hlsli"
#include "Sampler.hlsli"
#include "Trail.hlsli"

Texture2D noise2D : register(t30); // ノイズテクスチャ
Texture3D noise3D : register(t31); // ノイズテクスチャ

cbuffer TRAIL_CONSTANT_BUFFER : register(b13)
{
    // Alpha is used as the Rush color blend amount.
    float4 rushColor;
    float emissiveStrength;
    float3 padding;
}

float4 main(VS_OUT input) : SV_TARGET
{
    float2 uv = input.uv;

    // ノイズ
    float2 noiseUV = uv;
    noiseUV.x += elapsedTime * 0.8f;
    noiseUV.y += sin(uv.x * 15.0f + elapsedTime * 6.0f) * 0.05f;
    float noise =  noise2D.Sample(samplerStates[LINEAR],noiseUV).r;
    float mask = smoothstep(0.2f, 1.0f, noise);

    // トレイルの幅
    float center = abs(uv.y - 0.5f) * 2.0f;

    // 真ん中が1、端が0
    float width = saturate(1.0f - center * center);

    // 剣先ほど発光
    float tip = smoothstep(0.6f, 1.0f, uv.x);

    // Alpha
    float alpha = input.alpha;
    alpha *= width;
    alpha *= mask;

    // 色
    float3 col = lerp(
        float3(1.0f, 1.0f, 1.0f), // 根元：白
        float3(0.35f, 0.9f, 1.0f), // シアン
        uv.x);

    col = lerp(col,float3(0.15f, 0.35f, 1.0f), // 先端：青
        uv.x * uv.x);

    // 剣先だけ少し明るく
    col += float3(0.5f, 0.7f, 1.0f) * tip * 0.5f;

    const float3 rushTrailColor = rushColor.rgb * (0.75f + tip * 0.75f);
    col = lerp(col, rushTrailColor, saturate(rushColor.a));
    col *= emissiveStrength;

    return float4(col, alpha);
}
