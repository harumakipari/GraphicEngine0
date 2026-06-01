#include "PerlinWorleyNoise.hlsli"

RWTexture3D<float4> lowFreqPerlinWorley : register(u0);

#define HIGH_FREQ_WORLEY_DIMENSIONS 32
#define HIGH_FREQ_WORLEY_NUMTHREADS 8
[numthreads(HIGH_FREQ_WORLEY_NUMTHREADS, HIGH_FREQ_WORLEY_NUMTHREADS, HIGH_FREQ_WORLEY_NUMTHREADS)]

void main(uint3 DTid : SV_DISPATCHTHREADID)
{
    const float freq = 4.0;
    
    float3 uvw = (float3) (DTid) / HIGH_FREQ_WORLEY_DIMENSIONS;
    
    float4 color = 0;
    color.r = WorleyFbm(uvw, freq * 1.0);
    color.g = WorleyFbm(uvw, freq * 2.0);
    color.b = WorleyFbm(uvw, freq * 4.0);
    color.a = 1.0;

    lowFreqPerlinWorley[DTid] = color;
}