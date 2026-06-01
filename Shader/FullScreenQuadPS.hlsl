#include "FullScreenQuad.hlsli"

#define POINT 0
#define LINEAR 1
#define ANISOTROPHIC 2
SamplerState samplerStates[3] : register(s0);
Texture2D sceneTextureMaps : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    return sceneTextureMaps.Sample(samplerStates[LINEAR], pin.texcoord);
}

