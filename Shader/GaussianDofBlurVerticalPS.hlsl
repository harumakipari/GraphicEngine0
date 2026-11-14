#include "Sampler.hlsli"
Texture2D hdrColorBufferTexture : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    uint mipLevel = 0, width, height, numberOfLevels;
    hdrColorBufferTexture.GetDimensions(mipLevel, width, height, numberOfLevels);

    const float offset[3] = { 0.0, 1.3846153846, 3.2307692308 };
    const float weight[3] = { 0.2270270270, 0.3162162162, 0.0702702703 };

    float4 sampled_color = hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord) * weight[0];

    for (int i = 1; i < 3; i++)
    {
        sampled_color += hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord + float2(0.0, offset[i] / height)) * weight[i];
        sampled_color += hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord - float2(0.0, offset[i] / height)) * weight[i];
    }
    return sampled_color;
}
