#include "Sampler.hlsli"
Texture2D hdrColorBufferTexture : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    return hdrColorBufferTexture.Sample(samplerStates[LINEAR], texcoord, 0.0);
}
