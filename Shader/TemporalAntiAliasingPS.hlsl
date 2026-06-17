#include "FullScreenQuad.hlsli"
#include "Sampler.hlsli"
Texture2D CurrentColor : register(t0);
Texture2D Velocity : register(t1);
Texture2D HistoryColor : register(t2);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 vel = Velocity.Sample(samplerStates[LINEAR], pin.texcoord).xy;
    float2 historyUv = pin.texcoord - vel;

    float3 current = CurrentColor.Sample(samplerStates[LINEAR], pin.texcoord);
    //float3 history = HistoryColor.Sample(samplerStates[LINEAR], pin.texcoord);
    float3 history = HistoryColor.Sample(samplerStates[LINEAR], historyUv).rgb;
    return float4(lerp(current, history, 0.5), 1);
}
