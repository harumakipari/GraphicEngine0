#include "Bloom.hlsli"
#include "Sampler.hlsli"

// –¾‚é‚¢‚Æ‚±‚ë‚ğæ‚èo‚·
Texture2D hdrColorBufferTexture : register(t0);
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 sampledColor = hdrColorBufferTexture.Sample(samplerStates[POINT], texcoord);
    return float4(step(bloomExtractionThreshold /*–¾‚é‚¢‚Æ‚±‚ë‚ÌŠî€’l*/, max(sampledColor.r, max(sampledColor.g, sampledColor.b))) * sampledColor.rgb, sampledColor.a);
}
