#include "Bloom.hlsli"
#include "Sampler.hlsli"

// –¾‚é‚¢‚Æ‚±‚ë‚ğæ‚èo‚·
Texture2D hdr_color_buffer_texture : register(t0);
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 sampled_color = hdr_color_buffer_texture.Sample(samplerStates[POINT], texcoord);

#if 0
	return float4(step(bloom_extraction_threshold, dot(sampled_color.rgb, float3(0.299, 0.587, 0.114))) * sampled_color.rgb * effect_data.bloom_intensity, sampled_color.a);
#else
    return float4(step(bloomExtractionThreshold /*–¾‚é‚¢‚Æ‚±‚ë‚ÌŠî€’l*/, max(sampled_color.r, max(sampled_color.g, sampled_color.b))) * sampled_color.rgb, sampled_color.a);
#endif	
}
