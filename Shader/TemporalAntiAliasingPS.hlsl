#include "FullScreenQuad.hlsli"
#include "Sampler.hlsli"
Texture2D CurrentColor : register(t0);
Texture2D Velocity : register(t1);
Texture2D HistoryColor : register(t2);

float3 SampleCurrent(float2 uv)
{
    return CurrentColor.Sample(samplerStates[LINEAR], uv).rgb;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 vel = Velocity.Sample(samplerStates[LINEAR], pin.texcoord).xy;
    float2 historyUv = pin.texcoord - vel;
#if 0
    float3 current = CurrentColor.Sample(samplerStates[LINEAR], pin.texcoord);
    

    float3 history = HistoryColor.Sample(samplerStates[LINEAR], historyUv).rgb;
    return float4(lerp(current, history, 0.5), 1);
#else
    uint mipLevel = 0, width, height, numberOfLevel, levels;
    CurrentColor.GetDimensions(mipLevel, width, height, numberOfLevel);

    
    float3 current = SampleCurrent(pin.texcoord);
    float3 history = HistoryColor.Sample(samplerStates[LINEAR], historyUv).rgb;

    // --- Color Clamping（3x3 の近傍） ---
    float3 minColor = float3(9999, 9999, 9999);
    float3 maxColor = float3(-9999, -9999, -9999);

    float2 texel = 1.0 / float2(width, height);

    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            float2 uv = pin.texcoord + float2(x, y) * texel;
            float3 c = SampleCurrent(uv);
            minColor = min(minColor, c);
            maxColor = max(maxColor, c);
        }
    }

    float3 historyClamped = clamp(history, minColor, maxColor);

    // ------------------------------
    //Emission を守る処理を追加
    // ------------------------------
    float lumCurrent = max(current.r, max(current.g, current.b));
    float lumHistory = max(history.r, max(history.g, history.b));

// 明るいほど clamping を弱める
    float emissionFactor = saturate((lumCurrent - lumHistory) * 2.0);

// emissionFactor が大きいほど history をそのまま使う
    historyClamped = lerp(historyClamped, history, emissionFactor);
    
    // --- Accumulation（0.1 / 0.9） ---
    float3 output = current * 0.1 + historyClamped * 0.9;

    return float4(output, 1);
#endif

}
