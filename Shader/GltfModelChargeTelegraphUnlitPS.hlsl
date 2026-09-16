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

    const float3 telegraphColor = float3(1.0f, 0.12f, 0.0f);
    const float alpha = saturate(mask * m.pbrMetallicRoughness.baseColorFactor.a);
    return float4(telegraphColor, alpha);
}
