#include "FullScreenQuad.hlsli"
#include "Sampler.hlsli"

Texture2D sceneColorTexture : register(t0);

cbuffer RadialBlurConstants : register(b8)
{
    float2 blurCenter;
    float blurRadiusPixels;
    float maskRadiusPixels;

    float blurIntensity;
    int samplingCount;
    float2 screenSize;
};

static const int MAX_RADIAL_BLUR_SAMPLES = 32;

float4 main(VS_OUT pin) : SV_TARGET
{
    const float4 sourceColor = sceneColorTexture.Sample(samplerStates[LINEAR_CLAMP], pin.texcoord);
    const int clampedSamplingCount = clamp(samplingCount, 1, MAX_RADIAL_BLUR_SAMPLES);

    if (blurIntensity <= 0.0f || blurRadiusPixels <= 0.0f || clampedSamplingCount <= 1)
    {
        return sourceColor;
    }

    const float2 safeScreenSize = max(screenSize, float2(1.0f, 1.0f));
    const float2 centerDeltaPixels = (blurCenter - pin.texcoord) * safeScreenSize;
    const float centerDistancePixels = length(centerDeltaPixels);
    const float2 blurDirectionPixels = centerDistancePixels > 0.0001f
        ? (centerDeltaPixels / centerDistancePixels) * blurRadiusPixels
        : float2(0.0f, 0.0f);

    float4 blurredColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    [loop]
    for (int sampleIndex = 0; sampleIndex < MAX_RADIAL_BLUR_SAMPLES; ++sampleIndex)
    {
        if (sampleIndex >= clampedSamplingCount)
        {
            break;
        }

        const float sampleT = (float)sampleIndex / (float)(clampedSamplingCount - 1);
        const float2 sampleUv = pin.texcoord + (blurDirectionPixels * sampleT) / safeScreenSize;
        blurredColor += sceneColorTexture.Sample(samplerStates[LINEAR_CLAMP], sampleUv);
    }
    blurredColor /= (float)clampedSamplingCount;

    const float maskFeatherPixels = max(blurRadiusPixels, 1.0f);
    const float radialMask = smoothstep(maskRadiusPixels, maskRadiusPixels + maskFeatherPixels, centerDistancePixels);
    const float blendAmount = saturate(blurIntensity) * radialMask;
    return lerp(sourceColor, blurredColor, blendAmount);
}
