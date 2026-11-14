#include "ComputeParticle.hlsli"
#include "Sampler.hlsli"

Texture2D colorMap : register(t0);

float4 main(PS_IN pin) : SV_TARGET
{
    float4 col = colorMap.Sample(samplerStates[ANISOTROPIC], pin.texcoord) * pin.color;

    return float4(col.rgb * pin.emissive, col.a); 
}