#include "Sampler.hlsli"
#include "Constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer SKY_CONSTANT_BUFFER : register(b12)
{
    float3 topColor;
    float scrollSpeed;

    float3 bottomColor;
    float cloudIntensity;
    
    float3 sunColor;
    float sunSize;

    float3 cloudColor;
    float cloudThreshold;

    float starScale;
    float2 starOffset;
    float starIntensity;

    float3 moonColor;
    float moonRadius;

    float2 moonPos;
    float2 moonOffset;

    float3 startAuroraColor;
    float value;

    float3 endAuroraColor;
    float value1;
};

Texture2D<float4> noiseTexture : register(t21);
Texture2D<float4> starTexture : register(t22);

float Hash21(float2 p)
{
    p = frac(p * float2(443.8975, 397.2973));
    p += dot(p, p + 19.19);
    return frac(p.x * p.y);
}

float StarLayer(float2 uv, float scale, float threshold)
{
    float2 starUV = uv * scale;

    float2 id = floor(starUV);
    float2 gv = frac(starUV) - 0.5;

    float n = Hash21(id);

    float d = length(gv);

    float star = smoothstep(0.05, 0.0, d);

    float mask = step(threshold, n);

    float twinkle = sin(elapsedTime * 3 + n * 100) * 0.5 + 0.5;

    return star * mask * twinkle;
}
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

 // ---- 空のグラデーション（上半分のみ） ----
    // uv.y: 0.5～1.0 の部分 → newY: 0～1 にマッピング
    float newY = saturate((uv.y - 0.5) * 2.0);

    float3 sky = lerp(topColor, bottomColor, newY);
#if 0
    // 雲
    float2 cuv = uv /** 1.2*/ /*+ scrollSpeed * iTime*/;
    cuv.x += scrollSpeed * iTime;
    float cloudMask = noiseTexture.Sample(samplerStates[LINEAR], cuv).r;

    float cloud = smoothstep(cloudThreshold, cloudThreshold + 0.05, cloudMask);
    cloud *= cloudIntensity;

    //float star=starTexture.Sample()

    return float4(sky + cloud * 0.5, 1.0);
#else
    float3 color = sky;

// --- 星 ---
    //float twinkle = sin(elapsedTime * 5 + uv.x * 200 + uv.y * 300) * 0.5 + 0.5;
    //float star = pow(starTexture.Sample(samplerStates[LINEAR], uv * starScale + starOffset).r, 6) * twinkle;
    //color += star * starIntensity;

    float star1 = StarLayer(uv, 100, 0.98);
    float star2 = StarLayer(uv, 200, 0.985);
    float star3 = StarLayer(uv, 400, 0.99);

    float stars = star1 + star2 + star3;
    color += stars * starIntensity;
// --- 月 ---

    float aspect = iResolution.x / iResolution.y;

    float2 uvCorrected = float2((uv.x - 0.5) * aspect + 0.5, uv.y);

    float2 moonPosCorrected = float2((moonPos.x - 0.5) * aspect + 0.5, moonPos.y);

// 外側の大円（外側の光）
    float dOuter = length(uvCorrected - moonPosCorrected);
    float outer = 1.0 - smoothstep(moonRadius, moonRadius + 0.005, dOuter);

// 内側の円（影）
    //float2 innerCenter = moonPosCorrected + float2(0.05, 0.0); // 少し右にずらす
    float MoonOffsetX = min(1.0f, moonOffset.x * 10.0);
    float2 innerCenter = moonPosCorrected + float2(MoonOffsetX, moonOffset.y); // 少し右にずらす
    float dInner = length(uvCorrected - innerCenter);
    float inner = 1.0 - smoothstep(moonRadius, moonRadius + 0.005, dInner);

// 三日月マスク
    float crescent = saturate(outer - inner);

// 色
    color += crescent * moonColor;

// 輝き（optional）
    float glow = exp(-dOuter * 20.0);
    color += moonColor * glow * 0.2;
#endif
        // 雲
    float2 cuv = uv /** 1.2*/ /*+ scrollSpeed * iTime*/;
    cuv.x += scrollSpeed * elapsedTime;
    float cloudMask = noiseTexture.Sample(samplerStates[LINEAR], cuv).r;

    float cloud = smoothstep(cloudThreshold, cloudThreshold + 0.05, cloudMask);
    cloud *= cloudIntensity;

#if 1
    // オーロラ
    float2 auv = float2(uv.x /** 3.0*/, uv.y * 6.0 + elapsedTime * 0.2); //
    //float2 auv = uv /** 1.2*/ + scrollSpeed * iTime; // ノイズでオーロラの帯を作る
    float n = noiseTexture.Sample(samplerStates[LINEAR], auv).r;
    // sin でゆらぎ
    float wave = sin(uv.y * value + elapsedTime * value1) * 0.1;
    // 「横方向ノイズ」＋「ゆらぎ」で縦ストライプへ
    float auroraMask = smoothstep(0.48, 0.52, n + wave);

// 色グラデーション
    float3 auroraColor = lerp(
    float3(startAuroraColor),
    float3(endAuroraColor),
    uv.y
);


// 淡くブレンド
    auroraColor *= auroraMask /** verticalMask*/ * 0.7;

    color += auroraColor;


#endif
    return float4(color + cloud * 0.5, 1.0);


}