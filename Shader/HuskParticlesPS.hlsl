#include "HuskParticles.hlsli"
Texture2D texture_map : register(t0);

float4 main(GS_OUT pin) : SV_TARGET
{
    return pin.color;
}