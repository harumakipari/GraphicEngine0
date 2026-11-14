#include "Sampler.hlsli"
Texture2D hdrColorBufferTexture : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    uint mip_level = 0, width, height, numberOfLevels;
    hdrColorBufferTexture.GetDimensions(mip_level, width, height, numberOfLevels);

    const float offset[3] = { 0.0, 1.3846153846, 3.2307692308 };
    const float weight[3] = { 0.2270270270, 0.3162162162, 0.0702702703 };

    float4 sampledColor = hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord) * weight[0];
    for (int i = 1; i < 3; i++)
    {
        sampledColor += hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord + float2(0.0, offset[i] / height)) * weight[i];
        sampledColor += hdrColorBufferTexture.Sample(samplerStates[LINEAR_BORDER_BLACK], texcoord - float2(0.0, offset[i] / height)) * weight[i];
    }
    return sampledColor;
}
