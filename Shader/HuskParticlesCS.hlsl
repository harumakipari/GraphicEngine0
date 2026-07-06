#include "HuskParticles.hlsli"

RWStructuredBuffer<particle> particle_buffer : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DISPATCHTHREADID)
{
    uint id = dtid.x;
    if (id <= particle_count)
    {
        particle p = particle_buffer[id];
        p.position += p.normal * delta_time;
        particle_buffer[id] = p;
    }
}
