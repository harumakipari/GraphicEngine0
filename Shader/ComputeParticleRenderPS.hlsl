#include "ComputeParticle.hlsli"

Texture2D colorMap : register(t0);

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState samplerStates[3] : register(s0);

float4 main(PS_IN pin) : SV_TARGET
{
    float4 col = colorMap.Sample(samplerStates[ANISOTROPIC], pin.texcoord) * pin.color;

    return float4(col.rgb * pin.emissive, col.a); 
}