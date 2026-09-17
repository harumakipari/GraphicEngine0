#include "GltfModel.hlsli"
#include "Sampler.hlsli"

#define BASE_COLOR_TEXTURE 0

// ChargeTelegraphPlane専用のUnlit出力。
// BaseColor Textureは白黒の強度/Alphaマスクとして扱い、Lightingは使用しない。
Texture2D<float4> materialTextures[5] : register(t1);

float4 main(VS_OUT pin) : SV_TARGET0
{
    const MaterialConstants m = materials[material];

    float mask = 1.0f;
    if (m.pbrMetallicRoughness.basecolorTexture.index > -1)
    {
        // マスク用途のため、通常のBaseColor用ガンマ補正は行わない。
        mask = saturate(materialTextures[BASE_COLOR_TEXTURE]
            .Sample(samplerStates[ANISOTROPIC], pin.texcoord).r);
    }

    const float alpha = saturate(mask * m.pbrMetallicRoughness.baseColorFactor.a);
    // The mask's brightest band is the thin core; retain the existing mask
    // for alpha so black/zero-alpha texels remain transparent.
    const float core = smoothstep(0.72f, 0.95f, mask);
    const float3 glowColor = float3(1.0f, 0.08f, 0.0f);
    const float3 coreColor = float3(1.0f, 0.5f, 0.03f);
    return float4(lerp(glowColor, coreColor, core), alpha);
}
