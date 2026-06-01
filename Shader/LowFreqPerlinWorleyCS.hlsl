#include "PerlinWorleyNoise.hlsli"

RWTexture3D<float4> lowFreqPerlinWorley : register(u0);

#define LOW_FREQ_PERLIN_WORLEY_DIMENSIONS 128
#define LOW_FREQ_PERLIN_WORLEY_NUMTHREADS 8
[numthreads(LOW_FREQ_PERLIN_WORLEY_NUMTHREADS, LOW_FREQ_PERLIN_WORLEY_NUMTHREADS, LOW_FREQ_PERLIN_WORLEY_NUMTHREADS)]

void main( uint3 DTid : SV_DISPATCHTHREADID )
{
    const float freq = 4.0;
    
    float3 uvw = (float3) (DTid) / LOW_FREQ_PERLIN_WORLEY_DIMENSIONS;
    
    float pfbm = lerp(1.0, PerlinFbm(uvw, freq /*4.0*/, 7), 0.5);
    pfbm = abs(pfbm * 2.0 - 1.0); // billowy perlin noise
    
    float4 color = 0;
    color.g = WorleyFbm(uvw, freq * 1.0);
    color.b = WorleyFbm(uvw, freq * 2.0);
    color.a = WorleyFbm(uvw, freq * 4.0);
    color.r = Remap(pfbm, 0.0, 1.0, color.g, 1.0); // perlin-worley
    
    lowFreqPerlinWorley[DTid] = color;
}