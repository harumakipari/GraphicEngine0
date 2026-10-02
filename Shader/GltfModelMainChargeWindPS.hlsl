#include "GltfModel.hlsli"
#include "Sampler.hlsli"

#define BASE_COLOR_TEXTURE 0

Texture2D<float4> materialTextures[5] : register(t1);

cbuffer MAIN_CHARGE_WIND_CONSTANT_BUFFER : register(b6)
{
    float2 maskScrollSpeed;
    float mainChargeWindOpacity;
    float radialSpeed;
    float radialTiling;
    int maskFlowMode;
    float2 mainChargeWindPadding;
}

float4 main(VS_OUT pin) : SV_TARGET0
{
    const MaterialConstants m = materials[material];
    // Preserve the original Linear UV scroll. Radial maps the same mask into
    // polar coordinates, with positive speed propagating rings outward.
    float2 uv = pin.texcoord + maskScrollSpeed * elapsedTime;
    if (maskFlowMode == 1)
    {
        const float2 centeredUv = pin.texcoord - float2(0.5f, 0.5f);
        const float distanceFromCenter = length(centeredUv);
        const float2 radialDirection = distanceFromCenter > 0.0001f
            ? centeredUv / distanceFromCenter
            : float2(0.0f, 0.0f);
        const float radialPhase = frac(distanceFromCenter * max(radialTiling, 0.01f) -
            radialSpeed * elapsedTime);
        uv = float2(0.5f, 0.5f) + radialDirection * (radialPhase * 0.5f);
    }

    float mask = 1.0f;
    if (m.pbrMetallicRoughness.basecolorTexture.index > -1)
    {
        mask = saturate(materialTextures[BASE_COLOR_TEXTURE]
            .Sample(samplerStates[ANISOTROPIC], uv).r);
    }

    const float alpha = saturate(mask * saturate(mainChargeWindOpacity) *
        m.pbrMetallicRoughness.baseColorFactor.a);
    const float3 color = cpuColor.rgb * max(0.0f, emissionPower);
    return float4(color, alpha);
}
