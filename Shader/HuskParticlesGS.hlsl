#include "HuskParticles.hlsli"

StructuredBuffer<particle> particle_buffer : register(t9);

float random01(uint seed)
{
    return frac(sin((float)seed * 12.9898f) * 43758.5453f);
}

float getParticleLifetime(uint id)
{
    const float lifetimeRandom = random01(id * 3u + 31u);
    return lifetime * lerp(lifetime_min_multiplier, lifetime_max_multiplier, lifetimeRandom);
}

[maxvertexcount(4)]
void main(point VS_OUT input[1] : SV_POSITION, inout TriangleStream<GS_OUT> output)
{
    const float3 billboard[] =
    {
        float3(-1.0f, -1.0f, 0.0f), // Bottom-left corner
		float3(-1.0f, +1.0f, 0.0f), // Top-left corner
		float3(+1.0f, -1.0f, 0.0f), // Bottom-right corner
		float3(+1.0f, +1.0f, 0.0f), // Top-right corner
    };
    const float2 texcoords[] =
    {
        float2(0.0f, 1.0f), // Bottom-left 
		float2(0.0f, 0.0f), // Top-left
		float2(1.0f, 1.0f), // Bottom-right
		float2(1.0f, 0.0f), // Top-right
    };
	
    particle p = particle_buffer[input[0].vertex_id];
    if (p.state == 2)
    {
        return;
    }

    // Keep the displayed subset stable across frames while reducing silhouette density.
    const float displayRandom = random01(input[0].vertex_id * 3u + 47u);
    if (displayRandom >= saturate(display_ratio))
    {
        return;
    }

    float3 Z = normalize(p.normal);
    float3 X = normalize(cross(Z, float3(0, 1, 0)));
    float3 Y = normalize(cross(Z, X));
    row_major float3x3 R = { X, Y, Z };

	[unroll]
    for (uint vertex_index = 0; vertex_index < 4; ++vertex_index)
    {
        GS_OUT element;
        float3 corner_pos = billboard[vertex_index] * particle_size;

        element.position = mul(float4(p.position + mul(corner_pos, R), 1), viewProjection);
        const float particleLifetime = max(getParticleLifetime(input[0].vertex_id), 0.0001f);
        const float lifeRatio = saturate(p.age / particleLifetime);
        const float fadeStart = saturate(fade_start_ratio);
        const float fadeDenominator = max(1.0f - fadeStart, 0.0001f);
        const float fade = p.state == 1
            ? (lifeRatio >= fadeStart
                ? 1.0f - saturate((lifeRatio - fadeStart) / fadeDenominator)
                : 1.0f)
            : 1.0f;
        element.color = float4(p.color.rgb, p.color.a * fade);
        element.texcoord = texcoords[vertex_index];
        output.Append(element);
    }

    output.RestartStrip();
}
