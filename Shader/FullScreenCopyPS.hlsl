#include "FullScreenQuad.hlsli"
#include "Sampler.hlsli"
Texture2D SceneColor : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    return SceneColor.Sample(samplerStates[LINEAR], pin.texcoord);
}
