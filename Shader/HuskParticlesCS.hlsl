#include "HuskParticles.hlsli"

RWStructuredBuffer<particle> particle_buffer : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    uint id = dtid.x;
    if (id < particle_count)
    {
        particle p = particle_buffer[id];
        if (p.state == 0 &&
            p.normalizedHeight >= 1.0f - death_progress)
        {
            p.state = 1;
            p.velocity = p.normal * detach_speed;
            p.age = 0.0f;
        }
        if (p.state == 1)
        {
            p.velocity.y += gravity_ * delta_time;
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
