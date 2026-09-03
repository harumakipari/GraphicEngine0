#include "HuskParticles.hlsli"

RWStructuredBuffer<particle> particle_buffer : register(u0);

float random01(uint seed)
{
    return frac(sin((float)seed * 12.9898f) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    uint id = dtid.x;
    if (id < particle_count)
    {
        particle p = particle_buffer[id];
        if (p.state == 0)
        {
            p.age += delta_time;
            const float startDelay = random01(id * 3u + 11u) * max_start_delay;
            if (p.normalizedHeight >= 1.0f - death_progress &&
                p.age >= startDelay)
            {
                const float speedRandom = random01(id * 3u + 17u);
                const float speedMultiplier = lerp(
                    rise_speed_min_multiplier,
                    rise_speed_max_multiplier,
                    speedRandom);
                const float randomX = random01(id * 3u + 23u) * 2.0f - 1.0f;
                const float randomZ = random01(id * 3u + 29u) * 2.0f - 1.0f;
                p.state = 1;
                p.velocity = float3(
                    randomX * horizontal_random_speed,
                    rise_speed * speedMultiplier,
                    randomZ * horizontal_random_speed);
                p.age = 0.0f;
            }
        }
        if (p.state == 1)
        {
            p.position += p.velocity * delta_time;
            p.age += delta_time;
            if (p.age >= lifetime)
            {
                p.state = 2;
            }
        }
        particle_buffer[id] = p;
    }
}
