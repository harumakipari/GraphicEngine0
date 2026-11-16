#include "sprite.hlsli"

Texture2D colorMap : register(t0);
SamplerState pointSamplerState : register(s0);
SamplerState linerSamplerState : register(s1);
SamplerState anisotropicSamplerState : register(s2);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(anisotropicSamplerState, pin.texCoord);
    float alpha = color.a;
    color = float4(color.rgb, alpha) * pin.color;
    clip(color.a - 0.01f);
    return color;
}