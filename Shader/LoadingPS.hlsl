#include "Constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// LoadingScene-only controls. Bound at b12; shared Scene constants remain untouched.
cbuffer LOADING_PARTICLE_CONSTANT_BUFFER : register(b12)
{
    float particleSpawnOutsideDistance;
    float particleBezierCurveAmount;
    float particleStartDelayRange;
    float particleGatherStart;
    float particleGatherDuration;
    float particleGatherEase;
    float particleFinalClusterRadius;
    float particleFadeOutAlpha;
};

float Hash11(float value)
{
    return frac(sin(value * 127.1f) * 43758.5453123f);
}

float2 QuadraticBezier(float2 p0, float2 p1, float2 p2, float t)
{
    const float u = 1.0f - t;
    return u * u * p0 + 2.0f * u * t * p1 + t * t * p2;
}

float2 SpawnPointFromId(int particleId, float2 screenHalfExtent)
{
    const float id = float(particleId);
    const float entrance = Hash11(id * 1.71f);
    const float along = lerp(-0.92f, 0.92f, Hash11(id * 4.13f));
    const float outside = particleSpawnOutsideDistance * lerp(0.72f, 1.30f, Hash11(id * 7.91f));

    // Uneven cardinal-edge and corner entry positions. They are deliberately
    // not paired, so the composition never forms a symmetric ring.
    if (entrance < 0.22f) return float2(-screenHalfExtent.x - outside, along * screenHalfExtent.y);
    if (entrance < 0.43f) return float2(+screenHalfExtent.x + outside, along * screenHalfExtent.y);
    if (entrance < 0.63f) return float2(along * screenHalfExtent.x, +screenHalfExtent.y + outside);
    if (entrance < 0.82f) return float2(along * screenHalfExtent.x, -screenHalfExtent.y - outside);

    const float cornerX = Hash11(id * 12.57f) < 0.5f ? -1.0f : 1.0f;
    const float cornerY = Hash11(id * 17.29f) < 0.5f ? -1.0f : 1.0f;
    return float2(cornerX * (screenHalfExtent.x + outside * 0.85f),
                  cornerY * (screenHalfExtent.y + outside * 0.85f));
}

float4 main(VS_OUT pin) : SV_Target
{
    const float2 fullscreen = max(iResolution.xy, float2(1.0f, 1.0f));
    const float2 screenHalfExtent = float2(fullscreen.x / fullscreen.y, 1.0f);
    const float2 p = (pin.position.xy * 2.0f - fullscreen) / min(fullscreen.x, fullscreen.y);

    const int DOTS = 16;
    const float PARTICLE_GLOW = 0.0018f;
    const float CENTER_GLOW = 0.32f;
    const float3 COLOR = float3(0.58f, 0.88f, 1.0f);

    float f = 0.0f;
    for (int i = 0; i < DOTS; ++i)
    {
        const float id = float(i);
        const float2 p0 = SpawnPointFromId(i, screenHalfExtent);
        const float targetAngle = Hash11(id * 29.17f) * 6.28318531f;
        const float targetScale = particleFinalClusterRadius * lerp(0.35f, 1.0f, Hash11(id * 33.89f));
        const float2 p2 = float2(cos(targetAngle), sin(targetAngle)) * targetScale;

        const float2 inward = normalize(p2 - p0);
        const float2 perpendicular = float2(-inward.y, inward.x)
            * (Hash11(id * 41.33f) < 0.5f ? -1.0f : 1.0f);
        const float controlProgress = lerp(0.20f, 0.55f, Hash11(id * 47.11f));
        const float controlDistance = particleBezierCurveAmount * lerp(0.55f, 1.25f, Hash11(id * 53.67f));
        // P1 is a large per-ID offset rather than a small tangent drift.
        const float2 p1 = lerp(p0, p2, controlProgress) + perpendicular * controlDistance;

        const float startDelay = Hash11(id * 61.93f) * particleStartDelayRange;
        const float localTime = saturate((elapsedTime - particleGatherStart - startDelay)
            / max(particleGatherDuration, 0.001f));
        // Each delayed particle enters quickly, then decelerates into the cluster.
        const float gather = 1.0f - pow(1.0f - localTime, max(particleGatherEase, 0.01f));
        const float2 particlePosition = QuadraticBezier(p0, p1, p2, gather);

        const float distanceToParticle = max(length(p - particlePosition), 0.012f);
        f += PARTICLE_GLOW / distanceToParticle;
    }

    const float centerFadeStart = particleGatherStart + particleGatherDuration * 0.60f;
    const float centerFadeEnd = particleGatherStart + particleGatherDuration + particleStartDelayRange;
    const float centerFade = smoothstep(centerFadeStart, max(centerFadeEnd, centerFadeStart + 0.001f), elapsedTime);
    const float centerGlow = CENTER_GLOW * centerFade * exp(-dot(p, p) / (0.23f * 0.23f));
    const float light = 1.0f - exp(-(f + centerGlow));
    const float3 loadingColor = float3(0.001f, 0.003f, 0.008f) +
        COLOR * light * 0.8f;
    return float4(loadingColor * saturate(particleFadeOutAlpha), 1.0f);
}