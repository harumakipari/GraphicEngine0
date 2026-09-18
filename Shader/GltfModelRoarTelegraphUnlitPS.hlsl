#include "GltfModel.hlsli"
#include "Sampler.hlsli"

#define BASE_COLOR_TEXTURE 0

// Roar Telegraph only: the BaseColor texture's R channel is a crack mask.
// cpuColor supplies the hue and emissionPower supplies HDR glow strength.
Texture2D<float4> materialTextures[5] : register(t1);

float4 main(VS_OUT pin) : SV_TARGET0
{
    const MaterialConstants m = materials[material];

    float mask = 1.0f;
    if (m.pbrMetallicRoughness.basecolorTexture.index > -1)
    {
        mask = saturate(materialTextures[BASE_COLOR_TEXTURE]
            .Sample(samplerStates[ANISOTROPIC], pin.texcoord).r);
    }

    // Keep the existing Telegraph alpha convention: modelBrightness is an
    // alpha-fade offset where -1.0 is hidden and 0.0 is fully visible.
    const float alphaFade = saturate(1.0f + modelBrightness);
    const float alpha = saturate(mask * m.pbrMetallicRoughness.baseColorFactor.a * alphaFade);

    // Brighter mask cores retain the artist-authored crack shape while the
    // hue always comes from the per-component CPU tint.
    const float core = smoothstep(0.72f, 0.95f, mask);
    const float glowMultiplier = lerp(0.75f, 1.35f, core);
    const float glowStrength = max(0.0f, emissionPower);
    const float3 color = cpuColor.rgb * glowMultiplier * glowStrength;

    return float4(color, alpha);
}
