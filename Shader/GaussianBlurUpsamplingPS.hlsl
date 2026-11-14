#include "Bloom.hlsli"
#include "Sampler.hlsli"

static const uint downsampled_count = 6;
Texture2D downsampledTextures[downsampled_count] : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float3 sampledColor = 0;
	[unroll]
    for (uint downsampledIndex = 0; downsampledIndex < downsampled_count; ++downsampledIndex)
    {
        sampledColor += downsampledTextures[downsampledIndex].Sample(samplerStates[LINEAR], texcoord).xyz;
    }
    return float4(sampledColor * bloomIntensity, 1);
}
