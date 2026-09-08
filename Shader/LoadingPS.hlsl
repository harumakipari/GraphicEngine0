#include "Constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 main(VS_OUT pin) : SV_Target
{
    float2 fullscreen = max(iResolution.xy, float2(1.0, 1.0));
    float2 p = (pin.position.xy * 2.0 - fullscreen) / min(fullscreen.x, fullscreen.y);

    const int DOTS = 16;
    const float GATHER_START = 0.5; // Gather Start (seconds)
    const float GATHER_END = 2.5;   // Gather End (seconds)
    const float PARTICLE_GLOW = 0.0018; // Particle Glow: inverse-distance strength
    const float CENTER_GLOW = 0.32; // Center Glow: soft halo strength
    const float3 COLOR = float3(0.58, 0.88, 1.0);

    float gather = smoothstep(GATHER_START, GATHER_END, elapsedTime);
    float f = 0.0;
    for (int i = 0; i < DOTS; ++i)
    {
        // Fixed golden-angle phases separate particles even at time zero.
        float phase = float(i) * 2.39996323;
        float radius = lerp(0.14, 0.65, sqrt((float(i) + 0.5) / float(DOTS)));
        float2 initialPosition = float2(cos(phase), sin(phase)) * radius;
        float2 clusterPosition = initialPosition * 0.075;
        float2 drift = float2(sin(elapsedTime * 0.65 + phase),
                              cos(elapsedTime * 0.5 + phase * 1.3));
        float2 particlePosition = lerp(initialPosition, clusterPosition, gather)
                                + drift * 0.018 * (1.0 - gather);
        float distanceToParticle = max(length(p - particlePosition), 0.012);
        f += PARTICLE_GLOW / distanceToParticle;
    }

    float centerFade = smoothstep(1.7, GATHER_END, elapsedTime);
    float centerGlow = CENTER_GLOW * centerFade * exp(-dot(p, p) / (0.23 * 0.23));
    // Bounded exposure prevents a white-hot cluster, keeping the backdrop dark.
    float light = 1.0 - exp(-(f + centerGlow));
    return float4(float3(0.001, 0.003, 0.008) + COLOR * light * 0.8, 1.0);
}
